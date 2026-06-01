#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

/*
 * Cadences, boundary cadences and cadence-confirmed key changes on the five reference pieces (Ursatz's
 * framework-v1-corpus).
 *
 * Every cadence claim is checked for what the document promises: valid locations, note ids that exist, once each and
 * in order, an arrival and a dominant inside the claim's span, a type the library names, a key region that exists. The
 * key changes are checked against the cadences: a change into a new key is a modulation (or weak modulation) exactly
 * when a cadence arrives in the new region, a return takes the extent of its excursion, and cadence_confirmed marks the
 * modulations.
 *
 * The counts are what Ursatz's chain gives (the library's, not hand readings; the hand marks are Ursatz's own test):
 *
 *   Bach 114     two perfect authentic cadences, the P1 B3 result (m. 20 and m. 28 of the 3/4 score). The file declares
 *                4/4 for a 3/4 piece and the analyzer reads the file's meter (R-056), so they arrive at m. 15 beat 3.5
 *                and m. 21 beat 3.5 of its measures; the test names them by their time in whole notes, 117/8 and
 *                165/8.
 *   Clementi 36  hc 7, pac 3, iac 2, subverted 1, the P1 B6 result.
 *   Mozart 545   boundary cadences exist, and each is a half cadence.
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

static const Piece PIECES[] = {{"Bach 114", "Bach 114.mid"},
                               {"Clementi 36", "Clementi 36.mid"},
                               {"Mozart 545", "Mozart 545.mid"},
                               {"Field 1", "Field 1.mid"},
                               {"Field 5", "Field 5.mid"}};

static const char *const TYPES[] = {"perfect-authentic", "imperfect-authentic", "half",       "subverted",
                                    "plagal",            "deceptive",           "authentic-unspecified", "unknown"};

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static double rational_of(const cJSON *rational) {
    return member(rational, "num")->valuedouble / member(rational, "den")->valuedouble;
}

static bool rational_equals(const cJSON *rational, long num, long den) {
    return (long)member(rational, "num")->valuedouble * den == num * (long)member(rational, "den")->valuedouble;
}

static bool position_valid(const cJSON *position) {
    return cJSON_IsObject(position) && member(position, "measure")->valuedouble >= 0 &&
           member(position, "beat")->valuedouble >= 1;
}

static bool within(const cJSON *inner, const cJSON *outer) {
    return rational_of(member(inner, "start")) >= rational_of(member(outer, "start")) - 1e-12 &&
           rational_of(member(inner, "end")) <= rational_of(member(outer, "end")) + 1e-12;
}

static size_t count_type(const cJSON *cadences, const char *type) {
    size_t count = 0;
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        count += strcmp(member(cadence, "type")->valuestring, type) == 0 ? 1 : 0;
    }
    return count;
}

static void check_claims(const Piece *piece, const ImportedNotes *notes, const AnalysisReport *report,
                         const cJSON *document) {
    const cJSON *cadences = member(document, "cadences");
    const cJSON *regions = member(member(document, "keys"), "regions");
    CHECK((size_t)cJSON_GetArraySize(cadences) == report->cadence_count);
    double previous_arrival = -1.0;
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        const cJSON *span = member(cadence, "span");
        const cJSON *location = member(cadence, "location");
        const cJSON *arrival = member(cadence, "arrival");
        const cJSON *dominant = member(cadence, "dominant");
        CHECK(strcmp(member(cadence, "framework")->valuestring, "standard-harmony-v1") == 0);
        CHECK(position_valid(member(location, "start")) && position_valid(member(location, "end")));
        CHECK(position_valid(member(member(arrival, "location"), "start")) &&
              position_valid(member(member(arrival, "location"), "end")));
        CHECK(within(member(arrival, "span"), span) && within(member(dominant, "span"), span));
        CHECK(rational_of(member(member(dominant, "span"), "start")) <=
              rational_of(member(member(arrival, "span"), "start")));

        bool known = false;
        for (size_t t = 0; t < sizeof(TYPES) / sizeof(TYPES[0]); t++) {
            known = known || strcmp(member(cadence, "type")->valuestring, TYPES[t]) == 0;
        }
        CHECK(known);
        CHECK(strcmp(member(cadence, "label")->valuestring, member(cadence, "type")->valuestring) == 0);

        const cJSON *ids = member(cadence, "note_ids");
        CHECK(cJSON_GetArraySize(ids) > 0);
        double last_id = -1.0;
        const cJSON *id;
        cJSON_ArrayForEach(id, ids) {
            CHECK(id->valuedouble > last_id && id->valuedouble < (double)notes->count);
            last_id = id->valuedouble;
        }

        const cJSON *region = member(cadence, "region");
        CHECK(cJSON_IsNull(region) || (cJSON_IsNumber(region) && region->valuedouble < cJSON_GetArraySize(regions)));
        const cJSON *soprano = member(cadence, "soprano_degree");
        CHECK(cJSON_IsNull(soprano) || cJSON_IsString(soprano));
        const char *strength = member(cadence, "arrival_beat_strength")->valuestring;
        CHECK(strcmp(strength, "strong") == 0 || strcmp(strength, "medium") == 0 || strcmp(strength, "weak") == 0);
        if (cJSON_IsTrue(member(cadence, "boundary"))) {
            CHECK(strcmp(member(cadence, "type")->valuestring, "half") == 0);
        }

        double arrival_start = rational_of(member(member(arrival, "span"), "start"));
        CHECK(arrival_start >= previous_arrival);
        previous_arrival = arrival_start;
    }
    (void)piece;
}

/* A change into a new key is a modulation exactly when a cadence arrives in the new region (R-044); a return takes
   the extent of the excursion before it. */
static void check_key_changes(const cJSON *document, const AnalysisReport *report) {
    const cJSON *cadences = member(document, "cadences");
    const cJSON *regions = member(member(document, "keys"), "regions");
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        bool modulation = change->extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION ||
                          change->extent == ANALYSIS_KEY_CHANGE_MODULATION;
        if (change->is_return) {
            CHECK(i > 0 && change->extent == report->key_changes[i - 1].extent);
            CHECK(change->cadence_confirmed == report->key_changes[i - 1].cadence_confirmed);
            continue;
        }
        const cJSON *into = member(cJSON_GetArrayItem(regions, (int)change->to_region), "span");
        bool arrives = false;
        const cJSON *cadence;
        cJSON_ArrayForEach(cadence, cadences) {
            double arrival = rational_of(member(member(member(cadence, "arrival"), "span"), "start"));
            arrives = arrives || (arrival >= rational_of(member(into, "start")) - 1e-12 &&
                                  arrival < rational_of(member(into, "end")) - 1e-12);
        }
        CHECK(modulation == arrives);
        CHECK(change->cadence_confirmed == modulation);
    }
    /* the document lists what the report has */
    size_t tonicizations = 0;
    size_t modulations = 0;
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        bool is_modulation = change->extent == ANALYSIS_KEY_CHANGE_WEAK_MODULATION ||
                             change->extent == ANALYSIS_KEY_CHANGE_MODULATION;
        modulations += is_modulation ? 1 : 0;
        tonicizations += !is_modulation && !change->is_return ? 1 : 0;
    }
    CHECK((size_t)cJSON_GetArraySize(member(document, "tonicizations")) == tonicizations);
    CHECK((size_t)cJSON_GetArraySize(member(document, "modulations")) == modulations);
}

static bool has_cadence_at(const cJSON *cadences, const char *type, long num, long den) {
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        if (strcmp(member(cadence, "type")->valuestring, type) == 0 &&
            rational_equals(member(member(member(cadence, "arrival"), "span"), "start"), num, den)) {
            return true;
        }
    }
    return false;
}

static void check_counts(const Piece *piece, const cJSON *cadences) {
    if (strcmp(piece->name, "Bach 114") == 0) {
        CHECK(count_type(cadences, "perfect-authentic") == 2);
        CHECK(has_cadence_at(cadences, "perfect-authentic", 117, 8));
        CHECK(has_cadence_at(cadences, "perfect-authentic", 165, 8));
    } else if (strcmp(piece->name, "Clementi 36") == 0) {
        CHECK(count_type(cadences, "half") == 6);
        CHECK(count_type(cadences, "perfect-authentic") == 4);
        CHECK(count_type(cadences, "imperfect-authentic") == 2);
        CHECK(count_type(cadences, "subverted") == 3);
        CHECK(cJSON_GetArraySize(cadences) == 15);
    } else if (strcmp(piece->name, "Mozart 545") == 0) {
        size_t boundary = 0;
        const cJSON *cadence;
        cJSON_ArrayForEach(cadence, cadences) {
            boundary += cJSON_IsTrue(member(cadence, "boundary")) ? 1 : 0;
        }
        CHECK(boundary > 0);
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
        check_claims(piece, &notes, &report, document);
        check_key_changes(document, &report);
        check_counts(piece, member(document, "cadences"));
        printf("%s: %zu cadences\n", piece->name, report.cadence_count);
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
        fprintf(stderr, "test_cadences: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_cadences: ok\n");
    return 0;
}
