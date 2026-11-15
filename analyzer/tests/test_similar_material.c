#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include <ursatz_analyzer/corpus.h>
#include <ursatz_analyzer/similar_material.h>

#include "test_support.h"

/*
 * The similar material search over the "reference" corpus CMake builds in the build tree (the five framework-v1
 * pieces). The opening four soprano notes of Mozart 545 are the motive: they must come back first, at m. 1 with
 * score 1, and again where the piece returns to them; the same motive a fourth higher must score the same. "I IV V I"
 * must find Bach 114's cadences. The motive and progression results are compared with
 * tests/snapshots/similar.reference.txt (with UPDATE_SNAPSHOTS=1 in the environment, the snapshot is rewritten
 * instead). Run with --write-motive <file> to write the four-note MIDI motive the CLI test searches for.
 */

static int failures = 0;

#define CHECK(condition, ...)                                                                                          \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL line %d: ", __LINE__);                                                               \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            fprintf(stderr, "\n");                                                                                     \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

/* C5 E5 G5 B4, eighth notes at 480 ticks to the quarter: a MIDI file of one track. */
static bool write_motive_midi(const char *path) {
    static const unsigned char pitches[4] = {72, 76, 79, 71};
    unsigned char track[64];
    size_t n = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char on[] = {0x00, 0x90, pitches[i], 80, 0x81, 0x70, 0x80, pitches[i], 0}; /* delta 0; delta 240 */
        memcpy(track + n, on, sizeof on);
        n += sizeof on;
    }
    static const unsigned char end[] = {0x00, 0xFF, 0x2F, 0x00};
    memcpy(track + n, end, sizeof end);
    n += sizeof end;
    unsigned char header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0, 'M', 'T', 'r', 'k',
                              0, 0, 0, (unsigned char)n};
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    bool ok = fwrite(header, 1, sizeof header, file) == sizeof header && fwrite(track, 1, n, file) == n;
    return fclose(file) == 0 && ok;
}

static char work_dir[1024];

/* One line per match: "<score> <piece> m.<start> b.<beat> - m.<end> b.<beat> <dimension> <score>..., numerals <n>". */
static char *render(const SimilarResults *results) {
    size_t capacity = 512 + results->count * 512;
    char *text = malloc(capacity);
    if (!text) {
        return NULL;
    }
    size_t length = (size_t)snprintf(text, capacity, "%zu pieces searched, %zu matches\n", results->pieces_searched, results->count);
    for (size_t i = 0; i < results->count; i++) {
        const SimilarMatch *m = &results->matches[i];
        length += (size_t)snprintf(text + length, capacity - length, "%lu/%lu %s m.%d b.%g - m.%d b.%g", m->score.similarity.numerator,
                                   m->score.similarity.denominator, m->piece, m->start_measure, m->start_beat, m->end_measure,
                                   m->end_beat);
        for (size_t d = 0; d < m->score.dimension_score_count; d++) {
            length += (size_t)snprintf(text + length, capacity - length, " %s %lu/%lu",
                                       similarity_dimension_name(m->score.dimension_scores[d].dimension),
                                       m->score.dimension_scores[d].score.numerator, m->score.dimension_scores[d].score.denominator);
        }
        length += (size_t)snprintf(text + length, capacity - length, ", numerals %zu, notes %zu\n", m->numeral_matches, m->note_count);
    }
    return text;
}

/* The first four soprano notes of the Mozart 545 document, as a motive moved up by transpose semitones. */
static bool mozart_motive(SimilarQuery *query, int transpose) {
    char path[1100];
    snprintf(path, sizeof path, "%s/Mozart 545.mid.analysis.json", work_dir);
    char *json = test_read_file(path);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    SimilarNote *notes = calloc(4, sizeof *notes);
    size_t count = 0;
    long last_id = -1;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItem(cJSON_GetObjectItem(document, "outer_voices"), "soprano")) {
        const cJSON *id = cJSON_GetObjectItem(entry, "note_id");
        if (!notes || count == 4 || strcmp(cJSON_GetObjectItem(entry, "status")->valuestring, "present") != 0 ||
            (long)id->valuedouble == last_id) {
            continue;
        }
        last_id = (long)id->valuedouble;
        const cJSON *whole = cJSON_GetObjectItem(cJSON_GetObjectItem(entry, "duration"), "whole_notes");
        notes[count++] = (SimilarNote){(int)cJSON_GetObjectItem(cJSON_GetObjectItem(entry, "pitch"), "midi")->valuedouble + transpose,
                                       0, 1, (unsigned long)cJSON_GetObjectItem(whole, "num")->valuedouble,
                                       (unsigned long)cJSON_GetObjectItem(whole, "den")->valuedouble};
    }
    cJSON_Delete(document);
    if (count != 4) {
        free(notes);
        return false;
    }
    *query = (SimilarQuery){SIMILAR_QUERY_MOTIVE, notes, 4, NULL, 0};
    return true;
}

static void check_snapshot(const char *text) {
    char snapshot_path[1100];
    snprintf(snapshot_path, sizeof snapshot_path, "%s/similar.reference.txt", URSATZ_SNAPSHOT_DIR);
    const char *update = getenv("UPDATE_SNAPSHOTS");
    if (update && strcmp(update, "1") == 0) {
        CHECK(test_write_file(snapshot_path, text), "cannot write %s", snapshot_path);
        return;
    }
    char *expected = test_read_file(snapshot_path);
    CHECK(expected && strcmp(expected, text) == 0, "results differ from %s", snapshot_path);
    free(expected);
}

static void test_motive(void) {
    SimilarQuery query;
    CHECK(mozart_motive(&query, 0), "cannot read the Mozart 545 motive");
    SimilarResults results = {0};
    SimilarOptions options = similar_options_default();
    CHECK(similar_material_find(work_dir, &query, &options, &results) == SIMILAR_OK, "motive search failed");
    CHECK(results.pieces_searched == 5, "searched %zu pieces", results.pieces_searched);
    CHECK(results.count > 0 && results.count <= options.max_results, "%zu matches", results.count);
    if (results.count > 0) {
        const SimilarMatch *first = &results.matches[0];
        CHECK(strcmp(first->piece, "Mozart 545.mid") == 0 && first->start_measure == 1 && first->start_beat == 1.0,
              "first match is %s m.%d b.%g", first->piece, first->start_measure, first->start_beat);
        CHECK(first->score.similarity.numerator == first->score.similarity.denominator, "first score %lu/%lu",
              first->score.similarity.numerator, first->score.similarity.denominator);
        CHECK(first->note_count == 4 && first->notes[0].onset_numerator == 0 && first->notes[0].pitch == query.notes[0].pitch,
              "first match notes");
        bool returns = false;
        for (size_t i = 1; i < results.count; i++) {
            const SimilarMatch *m = &results.matches[i];
            returns = returns || (strcmp(m->piece, "Mozart 545.mid") == 0 && m->start_measure > 1 &&
                                  m->score.similarity.numerator == m->score.similarity.denominator);
        }
        CHECK(returns, "no full-score return of the motive in Mozart 545");
        for (size_t i = 1; i < results.count; i++) {
            const SimilarMatch *a = &results.matches[i - 1], *b = &results.matches[i];
            CHECK((unsigned __int128)a->score.similarity.numerator * b->score.similarity.denominator >=
                      (unsigned __int128)b->score.similarity.numerator * a->score.similarity.denominator,
                  "match %zu scores higher than the one before", i);
        }
    }
    char *text = render(&results);
    CHECK(text != NULL, "cannot render");

    SimilarQuery higher;
    SimilarResults higher_results = {0};
    CHECK(mozart_motive(&higher, 5), "cannot read the transposed motive");
    CHECK(similar_material_find(work_dir, &higher, &options, &higher_results) == SIMILAR_OK, "transposed search failed");
    char *higher_text = render(&higher_results);
    CHECK(text && higher_text && strcmp(text, higher_text) == 0, "a motive moved up a fourth scores differently:\n%s\n%s",
          text ? text : "", higher_text ? higher_text : "");
    free(higher_text);
    similar_results_destroy(&higher_results);
    similar_query_destroy(&higher);

    if (text) {
        char *progression_text = NULL;
        SimilarQuery progression;
        SimilarResults progression_results = {0};
        CHECK(similar_query_from_numerals("I IV V I", &progression) == SIMILAR_OK, "cannot read the progression");
        SimilarOptions wide = similar_options_default();
        wide.max_results = 100; /* Bach 114's cadences score lower than the best ten */
        CHECK(similar_material_find(work_dir, &progression, &wide, &progression_results) == SIMILAR_OK, "progression search failed");
        progression_text = render(&progression_results);
        size_t total = strlen(text) + (progression_text ? strlen(progression_text) : 0) + 64;
        char *both = malloc(total);
        CHECK(both && progression_text, "cannot render the progression");
        if (both && progression_text) {
            snprintf(both, total, "motive: Mozart 545 opening\n%sprogression: I IV V I\n%s", text, progression_text);
            check_snapshot(both);
        }
        bool bach = false;
        for (size_t i = 0; i < progression_results.count; i++) {
            bach = bach || strcmp(progression_results.matches[i].piece, "Bach 114.mid") == 0;
        }
        CHECK(bach, "no Bach 114 cadence among the matches of I IV V I");
        free(both);
        free(progression_text);
        similar_results_destroy(&progression_results);
        similar_query_destroy(&progression);
    }
    free(text);
    similar_results_destroy(&results);
    similar_query_destroy(&query);
}

static void test_queries(void) {
    SimilarQuery query;
    CHECK(similar_query_from_numerals("ii-V6/5-I", &query) == SIMILAR_OK && query.numeral_count == 3 &&
              strcmp(query.numerals[0], "ii") == 0 && strcmp(query.numerals[1], "V7") == 0 && strcmp(query.numerals[2], "I") == 0,
          "numerals read wrongly");
    similar_query_destroy(&query);
    CHECK(similar_query_from_numerals("I  IV\tV7 I", &query) == SIMILAR_OK && query.numeral_count == 4, "spaces and tabs");
    similar_query_destroy(&query);
    CHECK(similar_query_from_numerals("I banana", &query) == SIMILAR_ERR_QUERY, "a word is not a numeral");
    CHECK(similar_query_from_numerals("I", &query) == SIMILAR_ERR_QUERY, "one numeral is not a progression");
    CHECK(similar_query_from_numerals("", &query) == SIMILAR_ERR_QUERY, "no numerals");
    CHECK(similar_query_from_numerals(NULL, &query) == SIMILAR_ERR_ARGUMENT, "NULL text");

    char path[1100];
    snprintf(path, sizeof path, "%s/motive.mid", work_dir);
    CHECK(write_motive_midi(path), "cannot write %s", path);
    CHECK(similar_query_from_file(path, &query) == SIMILAR_OK && query.kind == SIMILAR_QUERY_MOTIVE && query.note_count == 4,
          "the motive file reads wrongly");
    if (query.note_count == 4) {
        CHECK(query.notes[0].pitch == 72 && query.notes[1].pitch == 76 && query.notes[2].pitch == 79 && query.notes[3].pitch == 71,
              "motive pitches %d %d %d %d", query.notes[0].pitch, query.notes[1].pitch, query.notes[2].pitch, query.notes[3].pitch);
        CHECK(query.notes[0].onset_numerator == 0 && query.notes[1].onset_numerator == 1 && query.notes[1].onset_denominator == 8,
              "motive onsets");
    }
    similar_query_destroy(&query);

    SimilarQuery short_query = {SIMILAR_QUERY_MOTIVE, calloc(1, sizeof(SimilarNote)), 1, NULL, 0};
    short_query.notes[0] = (SimilarNote){60, 0, 1, 1, 4};
    SimilarResults results = {0};
    CHECK(similar_material_find(work_dir, &short_query, NULL, &results) == SIMILAR_ERR_QUERY, "a one-note query is refused");
    similar_query_destroy(&short_query);

    CHECK(similar_query_from_numerals("I V", &query) == SIMILAR_OK, "I V");
    SimilarOptions options = similar_options_default();
    CHECK(similar_material_find("/nonexistent/corpus", &query, &options, &results) == SIMILAR_ERR_IO, "a missing corpus");
    options.max_results = 0;
    CHECK(similar_material_find(work_dir, &query, &options, &results) == SIMILAR_ERR_ARGUMENT, "no results asked for");
    options = similar_options_default();
    options.minimum_numerator = 3;
    options.minimum_denominator = 2;
    CHECK(similar_material_find(work_dir, &query, &options, &results) == SIMILAR_ERR_ARGUMENT, "a minimum above 1");
    options = similar_options_default();
    options.minimum_numerator = options.minimum_denominator = 1;
    CHECK(similar_material_find(work_dir, &query, &options, &results) == SIMILAR_OK, "minimum 1");
    for (size_t i = 0; i < results.count; i++) {
        CHECK(results.matches[i].score.similarity.numerator == results.matches[i].score.similarity.denominator, "minimum 1 kept %zu", i);
    }
    options.max_results = 2;
    options.minimum_numerator = 0;
    similar_results_destroy(&results);
    CHECK(similar_material_find(work_dir, &query, &options, &results) == SIMILAR_OK && results.count == 2, "max 2 gives %zu", results.count);
    similar_results_destroy(&results);
    similar_query_destroy(&query);

    /* A corpus output folder that is not one. */
    snprintf(path, sizeof path, "%s/not-a-corpus", work_dir);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/not-a-corpus/corpus-index.json", work_dir);
    CHECK(test_write_file(path, "{\"schema\": \"something-else\"}"), "cannot write the fake index");
    CHECK(similar_query_from_numerals("I V", &query) == SIMILAR_OK, "I V again");
    snprintf(path, sizeof path, "%s/not-a-corpus", work_dir);
    CHECK(similar_material_find(path, &query, NULL, &results) == SIMILAR_ERR_FORMAT, "a foreign index");
    similar_query_destroy(&query);
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--write-motive") == 0) {
        return write_motive_midi(argv[2]) ? 0 : 1;
    }
    snprintf(work_dir, sizeof work_dir, "%s", URSATZ_SIMILAR_WORK_DIR);
    char command[1100];
    snprintf(command, sizeof command, "rm -rf '%s'", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot clear %s\n", work_dir);
    }
    mkdir(work_dir, 0777);

    CorpusRunOptions run = {URSATZ_CORPORA_DIR "/reference", work_dir, false, false, NULL, NULL};
    CorpusRunSummary summary;
    if (corpus_run(&run, &summary) != CORPUS_RUN_OK || summary.analyzed != 5) {
        fprintf(stderr, "cannot analyze the reference corpus\n");
        return 1;
    }
    test_queries();
    test_motive();

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_similar_material: ok\n");
    return 0;
}
