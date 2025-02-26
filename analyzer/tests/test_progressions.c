#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"
#include "pipeline/progression_pipeline.h"

/*
 * Recurring progressions: the finder on hand-built chord lists, then the progressions section of two reference pieces
 * (Ursatz's framework-v1-corpus).
 *
 * The unit cases name the rules of progression_stage_collect: a held chord (the same base numeral again) counts once,
 * a run does not cross an unlabelled chord or a change of key region, a pattern needs two occurrences, a pattern
 * inside a longer one with the same occurrences is dropped, and no pattern is longer than six chords.
 *
 * The pieces are checked for what the document promises: counts and lengths agree with the pattern and its occurrences,
 * every occurrence's chords carry the pattern's numerals inside one key region, and the order is the rank.
 *
 *   test_progressions            the unit cases and the structure of Bach 114 and Mozart 545
 *   test_progressions bach-top   Bach 114's most common progression ends on I. The library's numerals give I > IV7
 *                                (6 occurrences, tied with ii > iii7), so this is expected to fail.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* ---- Hand-built chord lists ---- */

/* A report whose entries are the tokens of spec, separated by spaces: "<region>:<numeral>" for a labelled chord in
   that key region, "-" for a chord with no numeral. */
static AnalysisReport report_of(const char *spec) {
    AnalysisReport report;
    memset(&report, 0, sizeof(report));
    char *copy = strdup(spec);
    size_t count = 0;
    for (const char *p = spec; *p; p++) {
        count += (*p == ' ') ? 1 : 0;
    }
    count++;
    report.entries = calloc(count, sizeof(AnalysisReportEntry));
    char *save = NULL;
    for (char *token = strtok_r(copy, " ", &save); token; token = strtok_r(NULL, " ", &save)) {
        AnalysisReportEntry *entry = &report.entries[report.entry_count++];
        if (token[0] == '-') {
            continue;
        }
        entry->in_region = true;
        entry->region = (size_t)atoi(token);
        entry->roman.labelled = true;
        entry->roman.base_numeral = strdup(strchr(token, ':') + 1);
    }
    free(copy);
    return report;
}

static bool is_pattern(const AnalysisProgression *progression, const char *label) {
    char *actual = progression_label(progression);
    bool same = actual && strcmp(actual, label) == 0;
    free(actual);
    return same;
}

static void check_occurrence(const AnalysisProgression *progression, size_t index, size_t first, size_t last) {
    CHECK(index < progression->occurrence_count);
    if (index < progression->occurrence_count) {
        CHECK(progression->occurrences[index].first_entry == first);
        CHECK(progression->occurrences[index].last_entry == last);
    }
}

static void test_held_chords(void) {
    /* I I IV V V I I IV V I: held chords merge, so I IV V I occurs twice (entries 0-6 and 5-9), sharing the held I,
       and all of its parts are inside it. */
    AnalysisReport report = report_of("0:I 0:I 0:IV 0:V 0:V 0:I 0:I 0:IV 0:V 0:I");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count == 1);
    if (report.progression_count == 1) {
        CHECK(is_pattern(&report.progressions[0], "I > IV > V > I"));
        CHECK(report.progressions[0].length == 4);
        CHECK(report.progressions[0].occurrence_count == 2);
        check_occurrence(&report.progressions[0], 0, 0, 6);
        check_occurrence(&report.progressions[0], 1, 5, 9);
    }
    analysis_report_destroy(&report);
}

static void test_region_break(void) {
    /* A run does not cross from key region 0 into 1: V > I never occurs, I > V occurs once in each. */
    AnalysisReport report = report_of("0:I 0:V 1:I 1:V");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count == 1);
    if (report.progression_count == 1) {
        CHECK(is_pattern(&report.progressions[0], "I > V"));
        CHECK(report.progressions[0].occurrence_count == 2);
        check_occurrence(&report.progressions[0], 0, 0, 1);
        check_occurrence(&report.progressions[0], 1, 2, 3);
    }
    analysis_report_destroy(&report);
}

static void test_unlabelled_break(void) {
    /* A chord with no numeral breaks a run, in the same key region too. */
    AnalysisReport report = report_of("0:I 0:V - 0:I 0:V");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count == 1);
    if (report.progression_count == 1) {
        CHECK(is_pattern(&report.progressions[0], "I > V"));
        check_occurrence(&report.progressions[0], 0, 0, 1);
        check_occurrence(&report.progressions[0], 1, 3, 4);
    }
    analysis_report_destroy(&report);
}

static void test_containment(void) {
    /* I V I V I V: I > V occurs three times and stays. V > I, I > V > I and V > I > V occur twice, each only inside
       I > V > I > V (also twice), so only that one remains with I > V. */
    AnalysisReport report = report_of("0:I 0:V 0:I 0:V 0:I 0:V");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count == 2);
    if (report.progression_count == 2) {
        CHECK(is_pattern(&report.progressions[0], "I > V"));
        CHECK(report.progressions[0].occurrence_count == 3);
        CHECK(is_pattern(&report.progressions[1], "I > V > I > V"));
        CHECK(report.progressions[1].occurrence_count == 2);
    }
    analysis_report_destroy(&report);
}

static void test_rank_ties(void) {
    /* Equal counts rank the longer pattern first, then the earlier. I > IV occurs twice, vi > ii twice, I > IV > V
       twice (so I > IV is dropped as part of it, but vi > ii stays). */
    AnalysisReport report = report_of("0:I 0:IV 0:V 0:vi 0:ii 0:I 0:IV 0:V 0:vi 0:ii");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count >= 1);
    if (report.progression_count >= 1) {
        for (size_t i = 1; i < report.progression_count; i++) {
            const AnalysisProgression *before = &report.progressions[i - 1];
            const AnalysisProgression *after = &report.progressions[i];
            CHECK(before->occurrence_count > after->occurrence_count ||
                  (before->occurrence_count == after->occurrence_count && before->length >= after->length));
        }
    }
    analysis_report_destroy(&report);
}

static void test_length_limit(void) {
    /* Seven different chords twice: runs stop at six chords, so no progression is longer. */
    AnalysisReport report = report_of("0:I 0:ii 0:iii 0:IV 0:V 0:vi 0:vii 0:I 0:ii 0:iii 0:IV 0:V 0:vi 0:vii");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count > 0);
    for (size_t i = 0; i < report.progression_count; i++) {
        CHECK(report.progressions[i].length >= 2 && report.progressions[i].length <= 6);
    }
    analysis_report_destroy(&report);
}

static void test_no_repeat(void) {
    AnalysisReport report = report_of("0:I 0:IV 0:V 0:ii");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count == 0);
    analysis_report_destroy(&report);
    report = report_of("0:I");
    CHECK(progression_stage_collect(&report));
    CHECK(report.progression_count == 0);
    analysis_report_destroy(&report);
}

/* ---- Reference pieces ---- */

typedef struct {
    const char *name;
    const char *file;
} Piece;

static const Piece PIECES[] = {{"Bach 114", "Bach 114.mid"}, {"Mozart 545", "Mozart 545.mid"}};

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

/* The occurrence's chords, held repeats merged, carry the pattern's base numerals in one key region. */
static void check_occurrence_chords(const cJSON *document, const cJSON *progression, const cJSON *occurrence) {
    const cJSON *chords = member(document, "chords");
    const cJSON *pattern = member(progression, "pattern");
    int next = 0;
    const char *previous = NULL;
    double previous_index = -1.0;
    const cJSON *index;
    cJSON_ArrayForEach(index, member(occurrence, "chords")) {
        CHECK(index->valuedouble == previous_index + 1.0 || previous_index < 0.0);
        previous_index = index->valuedouble;
        const cJSON *roman = member(cJSON_GetArrayItem(chords, (int)index->valuedouble), "roman_numeral");
        const char *base = member(roman, "base_numeral") ? member(roman, "base_numeral")->valuestring : NULL;
        CHECK(base != NULL);
        CHECK(member(roman, "key_region")->valuedouble == member(occurrence, "region")->valuedouble);
        if (base && (previous == NULL || strcmp(previous, base) != 0)) {
            const cJSON *expected = cJSON_GetArrayItem(pattern, next);
            CHECK(expected && strcmp(expected->valuestring, base) == 0);
            next++;
        }
        previous = base;
    }
    CHECK(next == cJSON_GetArraySize(pattern));
}

static void check_progressions(const cJSON *document, const AnalysisReport *report) {
    const cJSON *progressions = member(document, "progressions");
    CHECK((size_t)cJSON_GetArraySize(progressions) == report->progression_count);
    CHECK(cJSON_GetArraySize(progressions) > 0);
    double previous_count = 1e9;
    double previous_length = 1e9;
    const cJSON *progression;
    cJSON_ArrayForEach(progression, progressions) {
        const cJSON *pattern = member(progression, "pattern");
        const cJSON *occurrences = member(progression, "occurrences");
        double count = member(progression, "count")->valuedouble;
        double length = member(progression, "length")->valuedouble;
        CHECK(length >= 2 && length <= 6 && (int)length == cJSON_GetArraySize(pattern));
        CHECK(count >= 2 && (int)count == cJSON_GetArraySize(occurrences));
        CHECK(count < previous_count || (count == previous_count && length <= previous_length));
        previous_count = count;
        previous_length = length;

        char label[512] = "";
        const cJSON *numeral;
        cJSON_ArrayForEach(numeral, pattern) {
            strcat(label, numeral == pattern->child ? "" : " > ");
            strcat(label, numeral->valuestring);
        }
        CHECK(strcmp(member(progression, "label")->valuestring, label) == 0);

        double previous_start = -1.0;
        const cJSON *occurrence;
        cJSON_ArrayForEach(occurrence, occurrences) {
            CHECK(strcmp(member(occurrence, "framework")->valuestring,
                         member(cJSON_GetArrayItem(member(document, "chords"), 0), "framework")->valuestring) == 0);
            CHECK(strcmp(member(occurrence, "label")->valuestring, label) == 0);
            CHECK(cJSON_IsObject(member(member(member(occurrence, "location"), "start"), "subdivision")));
            CHECK(cJSON_GetArraySize(member(occurrence, "note_ids")) > 0);
            const cJSON *uncertainty = member(occurrence, "uncertainty");
            CHECK(cJSON_IsNumber(uncertainty) && uncertainty->valuedouble >= 0.0 && uncertainty->valuedouble <= 1.0);
            double first_chord = cJSON_GetArrayItem(member(occurrence, "chords"), 0)->valuedouble;
            CHECK(first_chord > previous_start);
            previous_start = first_chord;
            check_occurrence_chords(document, progression, occurrence);
        }
    }
}

static cJSON *analyze(const Piece *piece, AnalysisReport *report, ImportedNotes *notes) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, piece->file);
    if (midi_pipeline_import_file(path, notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(notes, report)) {
        fprintf(stderr, "%s: cannot analyze %s\n", piece->name, path);
        failures++;
        return NULL;
    }
    char *json = analysis_document_render(path, notes, report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    return document;
}

static void run_piece(const Piece *piece) {
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document = analyze(piece, &report, &notes);
    if (document) {
        check_progressions(document, &report);
        printf("%s: %zu progressions\n", piece->name, report.progression_count);
        cJSON_Delete(document);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }
}

/* Bach 114's most common progression ends on the tonic. */
static void check_bach_top(void) {
    ImportedNotes notes;
    AnalysisReport report;
    cJSON *document = analyze(&PIECES[0], &report, &notes);
    if (!document) {
        return;
    }
    const cJSON *top = cJSON_GetArrayItem(member(document, "progressions"), 0);
    const cJSON *pattern = member(top, "pattern");
    CHECK(top != NULL);
    if (top) {
        printf("Bach 114: top progression %s (%d)\n", member(top, "label")->valuestring,
               (int)member(top, "count")->valuedouble);
        CHECK(strcmp(cJSON_GetArrayItem(pattern, cJSON_GetArraySize(pattern) - 1)->valuestring, "I") == 0);
    }
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "bach-top") == 0) {
        check_bach_top();
    } else {
        test_held_chords();
        test_region_break();
        test_unlabelled_break();
        test_containment();
        test_rank_ties();
        test_length_limit();
        test_no_repeat();
        for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
            run_piece(&PIECES[i]);
        }
    }
    if (failures > 0) {
        fprintf(stderr, "test_progressions: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_progressions: ok\n");
    return 0;
}
