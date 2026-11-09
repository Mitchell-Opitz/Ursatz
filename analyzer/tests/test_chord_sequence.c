#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/chord_sequence.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/style_labels.h>

#include <ursatz_chain/analysis_chain.h>

#include "test_support.h"

/*
 * The joined chord sequence (chord_sequence.h):
 *   - on the five reference pieces the sequence of a result and the sequence of its document are the same, and each
 *     chord is a run of document entries with one numeral, root and key region that no neighbouring chord continues,
 *   - a chord held across segments is one chord, a key region change splits a run, an entry with no numeral ends one,
 *   - the argument errors.
 *
 *   test_chord_sequence          the five pieces and the cases
 *   test_chord_sequence smoke    Bach 114 and the cases (the valgrind run)
 */

static const struct {
    const char *fixture;
    const char *slug;
} PIECES[] = {
    {"Bach 114.mid", "bach-114"}, {"Mozart 545.mid", "mozart-545"},   {"Clementi 36.mid", "clementi-36"},
    {"Field 1.mid", "field-1"},   {"Field 5.mid", "field-5"},
};

static int failures = 0;

#define CHECK(condition, ...)                                \
    do {                                                     \
        if (!(condition)) {                                  \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                    \
            fprintf(stderr, "\n");                           \
            failures++;                                      \
        }                                                    \
    } while (0)

static bool same_chord(const AnalyzerChord *a, const AnalyzerChord *b) {
    return strcmp(a->numeral, b->numeral) == 0 && a->root_pitch_class == b->root_pitch_class &&
           a->key_region == b->key_region && a->first_entry == b->first_entry && a->last_entry == b->last_entry &&
           rational_equals(a->start, b->start) && rational_equals(a->end, b->end);
}

static bool spans_equal(const cJSON *span, const char *edge, Rational expected) {
    const cJSON *fraction = cJSON_GetObjectItemCaseSensitive(span, edge);
    Rational value = rational_ratio((long long)test_json_number(fraction, "num"), (long long)test_json_number(fraction, "den"));
    return rational_equals(value, expected);
}

/* Each chord is entries first..last of the document that read as one numeral, root and key region, the chord starts and
   ends where those entries do, and no chord is the continuation of the one before it. Returns the number of chords that
   are held across more than one entry. */
static size_t check_against_entries(const char *slug, const cJSON *document, const AnalyzerChord *chords, size_t count) {
    const cJSON *entries = cJSON_GetObjectItemCaseSensitive(document, "chords");
    size_t held = 0;
    for (size_t i = 0; i < count; i++) {
        const AnalyzerChord *chord = &chords[i];
        CHECK(chord->first_entry <= chord->last_entry && chord->last_entry < (size_t)cJSON_GetArraySize(entries),
              "%s: chord %zu has entries %zu to %zu", slug, i, chord->first_entry, chord->last_entry);
        if (chord->first_entry > chord->last_entry || chord->last_entry >= (size_t)cJSON_GetArraySize(entries)) {
            return held;
        }
        CHECK(i == 0 || chord->first_entry > chords[i - 1].last_entry, "%s: chord %zu overlaps the one before", slug, i);
        held += chord->last_entry > chord->first_entry;
        for (size_t e = chord->first_entry; e <= chord->last_entry; e++) {
            const cJSON *entry = cJSON_GetArrayItem(entries, (int)e);
            const cJSON *roman = cJSON_GetObjectItemCaseSensitive(entry, "roman_numeral");
            char *numeral = style_label_numeral(test_json_string(roman, "base_numeral"));
            CHECK(numeral && strcmp(numeral, chord->numeral) == 0, "%s: chord %zu entry %zu numeral", slug, i, e);
            free(numeral);
            CHECK((long)test_json_number(roman, "key_region") == chord->key_region, "%s: chord %zu entry %zu region", slug,
                  i, e);
        }
        const cJSON *first = cJSON_GetArrayItem(entries, (int)chord->first_entry);
        const cJSON *last = cJSON_GetArrayItem(entries, (int)chord->last_entry);
        CHECK(spans_equal(cJSON_GetObjectItemCaseSensitive(first, "span"), "start", chord->start), "%s: chord %zu start", slug, i);
        CHECK(spans_equal(cJSON_GetObjectItemCaseSensitive(last, "span"), "end", chord->end), "%s: chord %zu end", slug, i);
        if (i > 0 && chord->first_entry == chords[i - 1].last_entry + 1 && chord->key_region == chords[i - 1].key_region) {
            CHECK(strcmp(chord->numeral, chords[i - 1].numeral) != 0 || chord->root_pitch_class != chords[i - 1].root_pitch_class,
                  "%s: chords %zu and %zu are one chord", slug, i - 1, i);
        }
    }
    return held;
}

static void check_piece(const char *fixture, const char *slug) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, fixture);
    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        CHECK(false, "%s: cannot import %s", slug, path);
        return;
    }
    AnalyzerResult *result = NULL;
    CHECK(analyzer_analyze(&notes, NULL, &result) == ANALYZER_OK && result, "%s: analyze", slug);
    if (result) {
        AnalyzerChord *from_result = NULL, *from_document = NULL;
        size_t result_count = 0, document_count = 0;
        char *json = analysis_document_render_result(fixture, result);
        cJSON *document = json ? cJSON_Parse(json) : NULL;
        CHECK(document != NULL, "%s: no document", slug);
        CHECK(analyzer_chord_sequence(result, &from_result, &result_count) == ANALYZER_OK, "%s: result sequence", slug);
        CHECK(json && analyzer_chord_sequence_from_document(json, &from_document, &document_count) == ANALYZER_OK,
              "%s: document sequence", slug);
        CHECK(result_count > 0, "%s: empty sequence", slug);
        CHECK(result_count == document_count, "%s: %zu chords from the result, %zu from the document", slug, result_count,
              document_count);
        for (size_t i = 0; i < result_count && i < document_count; i++) {
            CHECK(same_chord(&from_result[i], &from_document[i]), "%s: chord %zu differs", slug, i);
        }
        if (document) {
            size_t held = check_against_entries(slug, document, from_result, result_count);
            CHECK(held > 0, "%s: no chord is held across segments", slug);
        }
        analyzer_chord_sequence_free(from_result, result_count);
        analyzer_chord_sequence_free(from_document, document_count);
        cJSON_Delete(document);
        free(json);
    }
    analyzer_result_destroy(result);
    imported_notes_destroy(&notes);
}

/* ---- cases ---- */

#define MAX_CASE_ENTRIES 8

typedef struct {
    const char *numeral; /* NULL: the entry has no numeral */
    const char *root;
    int region;
    int start, end; /* in quarter notes */
} CaseEntry;

static char *case_document(const CaseEntry *entries, size_t count, int version) {
    char *text = malloc(4096);
    size_t used = (size_t)snprintf(text, 4096, "{\"schema_version\":%d,\"chords\":[", version);
    for (size_t i = 0; i < count; i++) {
        const CaseEntry *e = &entries[i];
        used += (size_t)snprintf(text + used, 4096 - used,
                                 "%s{\"root\":\"%s\",\"span\":{\"start\":{\"num\":%d,\"den\":4},\"end\":{\"num\":%d,\"den\":4}},"
                                 "\"roman_numeral\":{\"base_numeral\":%s%s%s,\"key_region\":%d}}",
                                 i ? "," : "", e->root, e->start, e->end, e->numeral ? "\"" : "", e->numeral ? e->numeral : "null",
                                 e->numeral ? "\"" : "", e->region);
    }
    snprintf(text + used, 4096 - used, "]}");
    return text;
}

static void check_case(const char *name, const CaseEntry *entries, size_t count, const size_t (*expected)[2], size_t expected_count) {
    char *json = case_document(entries, count, 11);
    AnalyzerChord *chords = NULL;
    size_t found = 0;
    CHECK(analyzer_chord_sequence_from_document(json, &chords, &found) == ANALYZER_OK, "%s: not read", name);
    CHECK(found == expected_count, "%s: %zu chords, expected %zu", name, found, expected_count);
    for (size_t i = 0; i < found && i < expected_count; i++) {
        CHECK(chords[i].first_entry == expected[i][0] && chords[i].last_entry == expected[i][1],
              "%s: chord %zu is entries %zu to %zu, expected %zu to %zu", name, i, chords[i].first_entry, chords[i].last_entry,
              expected[i][0], expected[i][1]);
        const CaseEntry *first = &entries[expected[i][0]], *last = &entries[expected[i][1]];
        CHECK(rational_equals(chords[i].start, rational_ratio(first->start, 4)) &&
                  rational_equals(chords[i].end, rational_ratio(last->end, 4)),
              "%s: chord %zu times", name, i);
    }
    analyzer_chord_sequence_free(chords, found);
    free(json);
}

static void check_cases(void) {
    /* A chord held across three segments is one chord, from the first start to the last end. */
    const CaseEntry held[] = {{"I", "C", 0, 0, 1}, {"I", "C", 0, 1, 2}, {"I", "C", 0, 2, 4}, {"V", "G", 0, 4, 6}};
    const size_t held_expected[][2] = {{0, 2}, {3, 3}};
    check_case("held", held, 4, held_expected, 2);

    /* The same numeral and root in another key region is another chord. */
    const CaseEntry region[] = {{"I", "C", 0, 0, 1}, {"I", "C", 1, 1, 2}, {"I", "C", 1, 2, 3}};
    const size_t region_expected[][2] = {{0, 0}, {1, 2}};
    check_case("region change", region, 3, region_expected, 2);

    /* The same numeral with another root is another chord. */
    const CaseEntry root[] = {{"V", "G", 0, 0, 1}, {"V", "D", 0, 1, 2}};
    const size_t root_expected[][2] = {{0, 0}, {1, 1}};
    check_case("root change", root, 2, root_expected, 2);

    /* An entry with no numeral ends the run: the chord before it is not continued after it. */
    const CaseEntry unlabelled[] = {{"I", "C", 0, 0, 1}, {NULL, "C", 0, 1, 2}, {"I", "C", 0, 2, 3}};
    const size_t unlabelled_expected[][2] = {{0, 0}, {2, 2}};
    check_case("unlabelled entry", unlabelled, 3, unlabelled_expected, 2);

    /* A document with no chords gives an empty sequence. */
    check_case("no chords", NULL, 0, NULL, 0);
}

static void check_arguments(void) {
    AnalyzerChord *chords = (AnalyzerChord *)0x1;
    size_t count = 7;
    CHECK(analyzer_chord_sequence(NULL, &chords, &count) == ANALYZER_ERR_ARGUMENT && chords == NULL && count == 0,
          "a NULL result");
    CHECK(analyzer_chord_sequence_from_document(NULL, &chords, &count) == ANALYZER_ERR_ARGUMENT, "a NULL document");
    CHECK(analyzer_chord_sequence_from_document("not json", &chords, &count) == ANALYZER_ERR_ARGUMENT && chords == NULL,
          "text that is not JSON");
    char *old_version = case_document(NULL, 0, 1);
    CHECK(analyzer_chord_sequence_from_document(old_version, &chords, &count) == ANALYZER_ERR_ARGUMENT,
          "another schema version");
    free(old_version);
    CHECK(analyzer_chord_sequence_from_document("{}", NULL, &count) == ANALYZER_ERR_ARGUMENT, "a NULL out");
    analyzer_chord_sequence_free(NULL, 0);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    size_t count = smoke ? 1 : sizeof(PIECES) / sizeof(PIECES[0]);
    for (size_t i = 0; i < count; i++) {
        check_piece(PIECES[i].fixture, PIECES[i].slug);
    }
    check_cases();
    check_arguments();
    if (failures == 0) {
        printf("chord sequence: ok\n");
    }
    return failures == 0 ? 0 : 1;
}
