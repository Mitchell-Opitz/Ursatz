#include <stdarg.h>

#include "framework_v1_chain.h"
#include "framework_v1_deviations.h"

#include "corpus/corpus_api.h"
#include "query/corpus_search.h"
#include "stats_test_util.h"

/*
 * Similarity search over the five Framework v1 reference pieces.
 *
 * THE GLUE. Each piece goes through the full chain (framework_v1_chain.h). The soprano line it found (outer_voice_extract) gives the piece's
 * melody: each time the soprano moves to a new note, the directed semitone interval from the previous one. Those intervals are persisted the way
 * interval analysis persists them (claim value type CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, one signed long each), each carrying the piece's file name
 * as its context_id and in the piece's order. The chain makes no durations or motif patterns to persist, so those are not searched.
 *
 * THE CHECKS, per piece and for all five as one Corpus. A query is a slice of the piece's own intervals at a known place:
 *   - it finds that place at similarity 1, and its results at similarity 1 are exactly the windows a brute-force comparison of the stored
 *     intervals finds, in piece order then window start;
 *   - a transposed copy of the same notes (every pitch moved the same number of semitones) has the same intervals and the same contour, so it
 *     finds the same places at 1 on the interval and contour dimensions;
 *   - a minimum above every score is NO_DATA, and a model that counts rhythm is refused (no durations are persisted);
 *   - the same search twice gives the same results.
 * The rhythm-augmentation case lives in test_similarity_scoring.c: it needs durations, which are not persisted.
 */

#define MAX_INTERVALS 4096

typedef struct {
    long intervals[MAX_INTERVALS];
    long pitches[MAX_INTERVALS + 1];
    size_t interval_count;
    size_t pitch_count;
} Melody;

/* The soprano's melody: its pitch each time the note changes, and the interval from each to the next. */
static void extract_melody(const Chain *c, Melody *melody) {
    melody->pitch_count = 0;
    melody->interval_count = 0;
    const EntityID *previous_note = NULL;
    for (size_t i = 0; i < c->lines.count; i++) {
        const OuterVoiceEntry *entry = &c->lines.soprano[i];
        if (!entry->present) {
            continue;
        }
        if (previous_note != NULL && entity_id_equals(previous_note, entry->note_id)) {
            continue; /* the same note continuing into the next segment */
        }
        assert(melody->pitch_count < MAX_INTERVALS);
        melody->pitches[melody->pitch_count++] = pitch_numeric_value_position_numerator(entry->pitch);
        previous_note = entry->note_id;
    }
    for (size_t i = 0; i + 1 < melody->pitch_count; i++) {
        melody->intervals[melody->interval_count++] = melody->pitches[i + 1] - melody->pitches[i];
    }
}

static void persist_melody(PersistenceHandle *persistence, const char *piece_ref, const Melody *melody) {
    char id[160];
    for (size_t i = 0; i < melody->interval_count; i++) {
        snprintf(id, sizeof(id), "%s-interval-%zu", piece_ref, i);
        stats_store_claim(persistence, id, CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, &melody->intervals[i], sizeof(long), piece_ref);
    }
}

/* Result window starts in a piece, from a result's first-interpretation id "<piece>-interval-<start>". */
static size_t start_of(const SearchResult *result) {
    const char *id = search_result_matched_object_id(result);
    const char *marker = strstr(id, "-interval-");
    assert(marker != NULL);
    return (size_t)strtoul(marker + strlen("-interval-"), NULL, 10);
}

static int equal_window(const long *a, const long *b, size_t n) {
    return memcmp(a, b, n * sizeof(long)) == 0;
}

/* Every result at similarity 1 from `piece_index`'s stored intervals must be exactly the brute-force exact matches. */
static void check_exact_results(const CorpusSearchResults *results, const Melody *melodies, const char *const *refs,
                                size_t piece_count, const long *query, size_t n) {
    size_t next = 0;
    for (size_t p = 0; p < piece_count; p++) {
        const Melody *melody = &melodies[p];
        for (size_t start = 0; start + n <= melody->interval_count; start++) {
            if (!equal_window(&melody->intervals[start], query, n)) {
                continue;
            }
            assert(next < results->count);
            const SearchResult *result = results->results[next++];
            assert(strcmp(search_result_piece_ref(result), refs[p]) == 0);
            assert(start_of(result) == start);
            assert(search_result_similarity_numerator(result) == 1 && search_result_similarity_denominator(result) == 1);
        }
    }
    assert(next == results->count);
}

int main(void) {
    const char *path = "test_corpus_similarity_search.sqlite";
    remove(path);
    CorpusStoreHandle *corpus = NULL;
    assert(corpus_api_open(path, &corpus) == CORPUS_API_OK);
    PersistenceHandle *persistence = NULL;
    assert(persistence_open(path, &persistence) == PERSISTENCE_OK);
    assert(corpus_api_create_corpus(corpus, "all-five", "Framework v1 reference set") == CORPUS_API_OK);

    static Melody melodies[5];
    const char *refs[5];
    for (size_t i = 0; i < 5; i++) {
        Chain c;
        fv1_chain_run(&c, &FORM_PIECES[i]);
        refs[i] = FORM_PIECES[i].piece.midi;
        extract_melody(&c, &melodies[i]);
        printf("%s: %zu soprano notes, %zu intervals persisted\n", refs[i], melodies[i].pitch_count, melodies[i].interval_count);
        assert(melodies[i].interval_count >= 12); /* enough to search */
        fv1_chain_destroy(&c);
        persist_melody(persistence, refs[i], &melodies[i]);

        char corpus_id[64];
        snprintf(corpus_id, sizeof(corpus_id), "piece-%zu", i);
        assert(corpus_api_create_corpus(corpus, corpus_id, refs[i]) == CORPUS_API_OK);
        assert(corpus_api_add_piece_ref(corpus, corpus_id, refs[i]) == CORPUS_API_OK);
        assert(corpus_api_add_piece_ref(corpus, "all-five", refs[i]) == CORPUS_API_OK);
    }

    SimilarityDimensionWeight interval_and_contour[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_CONTOUR, 1, 1}};
    SimilarityModel *model = NULL;
    assert(similarity_model_create(interval_and_contour, 2, &model) == SIMILARITY_MODEL_OK);

    for (size_t i = 0; i < 5; i++) {
        const Melody *melody = &melodies[i];
        size_t n = 6;
        size_t known_start = melody->interval_count / 3;
        assert(known_start + n <= melody->interval_count);
        const long *slice = &melody->intervals[known_start];
        char corpus_id[64];
        snprintf(corpus_id, sizeof(corpus_id), "piece-%zu", i);

        /* The motive at a known place is found there at similarity 1; its exact matches are the brute-force ones. */
        SimilarityInput query = {slice, n, NULL, 0, NULL, 0};
        CorpusSearchResults results = {NULL, 0};
        assert(corpus_search_melodic(corpus, persistence, corpus_id, &query, model, 1, 1, SIZE_MAX, &results) == CORPUS_SEARCH_OK);
        check_exact_results(&results, melody, &refs[i], 1, slice, n);
        int found = 0;
        for (size_t r = 0; r < results.count; r++) {
            found |= start_of(results.results[r]) == known_start;
            assert(search_result_evidence_count(results.results[r]) == 2);
            assert(search_result_evidence_at(results.results[r], 0)->score_numerator == 1);
            assert(search_result_evidence_at(results.results[r], 1)->score_numerator == 1);
        }
        assert(found);

        /* A transposed copy: the same notes moved by seven semitones have the same intervals (read back from the moved pitches), so the
           same places come back at 1 on interval and contour. */
        long moved[8];
        long moved_intervals[8];
        for (size_t k = 0; k <= n; k++) {
            moved[k] = melody->pitches[known_start + k] + 7;
        }
        for (size_t k = 0; k < n; k++) {
            moved_intervals[k] = moved[k + 1] - moved[k];
        }
        assert(equal_window(moved_intervals, slice, n));
        SimilarityInput transposed = {moved_intervals, n, NULL, 0, NULL, 0};
        CorpusSearchResults transposed_results = {NULL, 0};
        assert(corpus_search_melodic(corpus, persistence, corpus_id, &transposed, model, 1, 1, SIZE_MAX, &transposed_results) ==
               CORPUS_SEARCH_OK);
        assert(transposed_results.count == results.count);
        for (size_t r = 0; r < results.count; r++) {
            assert(start_of(transposed_results.results[r]) == start_of(results.results[r]));
        }
        corpus_search_results_free(&transposed_results);

        /* The same search twice gives the same results. */
        CorpusSearchResults again = {NULL, 0};
        assert(corpus_search_melodic(corpus, persistence, corpus_id, &query, model, 1, 1, SIZE_MAX, &again) == CORPUS_SEARCH_OK);
        assert(again.count == results.count);
        for (size_t r = 0; r < results.count; r++) {
            assert(start_of(again.results[r]) == start_of(results.results[r]));
        }
        corpus_search_results_free(&again);
        corpus_search_results_free(&results);

        /* Nothing is that far from the melody: a minimum no window reaches is no data. */
        long impossible[6] = {1000, 1000, 1000, 1000, 1000, 1000};
        SimilarityInput unlike = {impossible, 6, NULL, 0, NULL, 0};
        assert(corpus_search_melodic(corpus, persistence, corpus_id, &unlike, model, 1, 1, SIZE_MAX, &results) == CORPUS_SEARCH_NO_DATA);
    }

    /* All five as one Corpus: results by piece order, each piece's exact matches in start order. */
    {
        const Melody *first = &melodies[2];
        size_t n = 6;
        size_t known_start = first->interval_count / 2;
        const long *slice = &first->intervals[known_start];
        SimilarityInput query = {slice, n, NULL, 0, NULL, 0};
        CorpusSearchResults results = {NULL, 0};
        assert(corpus_search_melodic(corpus, persistence, "all-five", &query, model, 1, 1, SIZE_MAX, &results) == CORPUS_SEARCH_OK);
        check_exact_results(&results, melodies, refs, 5, slice, n);
        int found = 0;
        for (size_t r = 0; r < results.count; r++) {
            found |= strcmp(search_result_piece_ref(results.results[r]), refs[2]) == 0 && start_of(results.results[r]) == known_start;
        }
        assert(found);
        corpus_search_results_free(&results);

        /* Rhythm cannot be searched: no durations are persisted. */
        SimilarityDimensionWeight with_rhythm[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_RHYTHM, 1, 1}};
        SimilarityModel *rhythm_model = NULL;
        assert(similarity_model_create(with_rhythm, 2, &rhythm_model) == SIMILARITY_MODEL_OK);
        assert(corpus_search_melodic(corpus, persistence, "all-five", &query, rhythm_model, 0, 1, 10, &results) ==
               CORPUS_SEARCH_ERR_DIMENSION_UNAVAILABLE);
        similarity_model_destroy(rhythm_model);
    }

    similarity_model_destroy(model);
    persistence_close(persistence);
    corpus_api_close(corpus);
    remove(path);
    printf("test_corpus_similarity_search: ok\n");
    return 0;
}
