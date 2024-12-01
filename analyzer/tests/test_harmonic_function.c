#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

/*
 * Harmonic function, six-fours and prolongations on Bach 114 and Mozart 545 (Ursatz's framework-v1-corpus).
 *
 * Every chord the library gives a Roman numeral also has a function; V and vii° are dominants, ii and IV strong
 * predominants, iii and vi weak ones (diatonic chords only: an applied chord is a dominant toward its target); an
 * applied chord names its target degree. A predominant strength is given exactly to a diatonic predominant. A
 * six-four has one of the library's types. Prolongations name an entry range that exists, in order, with a function
 * the library prolongs and the library's reasons. Mozart 545 has at least one cadential six-four.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct {
    const char *name;
    const char *file;
} Piece;

static const Piece PIECES[] = {{"Bach 114", "Bach 114.mid"}, {"Mozart 545", "Mozart 545.mid"}};

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static bool in_list(const char *value, const char *const *list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(value, list[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void check_chords(const cJSON *document, size_t *cadential_six_fours) {
    static const char *const FUNCTIONS[] = {"tonic", "predominant", "dominant"};
    static const char *const SIX_FOURS[] = {"cadential", "passing", "neighbor", "arpeggiating"};
    const cJSON *chords = member(document, "chords");
    size_t labelled = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        const cJSON *roman = member(chord, "roman_numeral");
        const cJSON *function = member(chord, "function");
        const cJSON *six_four = member(chord, "six_four");
        const char *base = cJSON_IsString(member(roman, "base_numeral")) ? member(roman, "base_numeral")->valuestring : NULL;
        bool applied = cJSON_IsObject(member(roman, "applied"));
        if (base) {
            labelled++;
            CHECK(cJSON_IsObject(function));
        }
        if (six_four) {
            CHECK(in_list(member(six_four, "type")->valuestring, SIX_FOURS, 4));
            *cadential_six_fours += strcmp(member(six_four, "type")->valuestring, "cadential") == 0 ? 1 : 0;
        }
        if (!function) {
            continue;
        }
        const char *name = member(function, "name")->valuestring;
        const cJSON *predominant = member(function, "predominant");
        CHECK(in_list(name, FUNCTIONS, 3));
        CHECK(strcmp(member(function, "framework")->valuestring, "standard-harmony-v1") == 0);
        CHECK(member(function, "uncertainty")->valuedouble >= 0.0 && member(function, "uncertainty")->valuedouble <= 1.0);
        if (applied) {
            CHECK(strcmp(name, "dominant") == 0);
            CHECK(cJSON_IsNumber(member(function, "applied_to")));
            CHECK(member(function, "applied_to")->valueint == member(member(roman, "applied"), "target_degree")->valueint);
            CHECK(cJSON_IsNull(predominant));
            continue;
        }
        CHECK(cJSON_IsNull(member(function, "applied_to")));
        CHECK(strcmp(name, "predominant") == 0 ? cJSON_IsString(predominant) : cJSON_IsNull(predominant));
        if (!base) {
            continue;
        }
        if (strcmp(base, "V") == 0 || strcmp(base, "V7") == 0 || strncmp(base, "vii", 3) == 0) {
            CHECK(strcmp(name, "dominant") == 0);
        }
        if (strcmp(base, "ii") == 0 || strcmp(base, "IV") == 0) {
            CHECK(strcmp(name, "predominant") == 0);
            CHECK(cJSON_IsString(predominant) && strcmp(predominant->valuestring, "strong") == 0);
        }
        if (strcmp(base, "iii") == 0 || strcmp(base, "vi") == 0) {
            CHECK(strcmp(name, "predominant") == 0);
            CHECK(cJSON_IsString(predominant) && strcmp(predominant->valuestring, "weak") == 0);
        }
    }
    CHECK(labelled > 0);
}

static void check_prolongations(const cJSON *document, const AnalysisReport *report) {
    static const char *const REASONS[] = {"passing-six-four", "neighbor-six-four", "arpeggiating-six-four",
                                          "tonic-line", "applied"};
    const cJSON *prolongations = member(document, "prolongations");
    CHECK(cJSON_IsArray(prolongations));
    CHECK((size_t)cJSON_GetArraySize(prolongations) == report->prolongation_count);
    double previous = -1.0;
    const cJSON *prolongation;
    cJSON_ArrayForEach(prolongation, prolongations) {
        double first = member(prolongation, "first_chord")->valuedouble;
        double last = member(prolongation, "last_chord")->valuedouble;
        CHECK(first > previous && last > first && last < (double)report->entry_count);
        previous = first;
        const char *function = member(prolongation, "function")->valuestring;
        CHECK(strcmp(function, "tonic") == 0 || strcmp(function, "dominant") == 0);
        CHECK(strcmp(member(prolongation, "framework")->valuestring, "standard-harmony-v1") == 0);
        CHECK(cJSON_GetArraySize(member(prolongation, "note_ids")) > 0);
        const cJSON *reasons = member(prolongation, "reasons");
        CHECK(cJSON_GetArraySize(reasons) > 0);
        const cJSON *reason;
        cJSON_ArrayForEach(reason, reasons) {
            CHECK(in_list(reason->valuestring, REASONS, 5));
        }
    }
}

static void run_piece(const Piece *piece) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, piece->file);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "%s: cannot analyze %s\n", piece->name, path);
        failures++;
        return;
    }
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    if (document) {
        size_t cadential = 0;
        check_chords(document, &cadential);
        check_prolongations(document, &report);
        if (strcmp(piece->name, "Mozart 545") == 0) {
            CHECK(cadential >= 1);
        }
        printf("%s: %zu cadential six-fours, %zu prolongations\n", piece->name, cadential, report.prolongation_count);
        cJSON_Delete(document);
    }
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        run_piece(&PIECES[i]);
    }
    if (failures > 0) {
        fprintf(stderr, "test_harmonic_function: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_harmonic_function: ok\n");
    return 0;
}
