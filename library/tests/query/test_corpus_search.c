#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../corpus_stats/stats_test_util.h"
#include "query/corpus_search.h"

/*
 * Corpus search over persisted interval claims, on small corpora whose scores
 * can be worked by hand. A piece's claims are its persisted intervals in store
 * order; ids are "<piece>-<index>", so the matched window can be read off the
 * result.
 */

typedef struct {
    CorpusStoreHandle *corpus;
    PersistenceHandle *persistence;
    const char *path;
} Store;

static Store open_store(const char *path) {
    Store store = {NULL, NULL, path};
    remove(path);
    assert(corpus_api_open(path, &store.corpus) == CORPUS_API_OK);
    assert(persistence_open(path, &store.persistence) == PERSISTENCE_OK);
    assert(corpus_api_create_corpus(store.corpus, "corpus-1", "Test corpus") == CORPUS_API_OK);
    return store;
}

static void close_store(Store *store) {
    persistence_close(store->persistence);
    corpus_api_close(store->corpus);
    remove(store->path);
}

/* Persists a piece's intervals as claims "<piece>-0", "<piece>-1", ... and adds the piece to the corpus. */
static void add_piece(Store *store, const char *piece, const long *intervals, size_t count) {
    char id[64];
    for (size_t i = 0; i < count; i++) {
        snprintf(id, sizeof(id), "%s-%zu", piece, i);
        stats_store_claim(store->persistence, id, CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, &intervals[i], sizeof(long), piece);
    }
    assert(corpus_api_add_piece_ref(store->corpus, "corpus-1", piece) == CORPUS_API_OK);
}

static SimilarityModel *make_model(const SimilarityDimensionWeight *weights, size_t count) {
    SimilarityModel *model = NULL;
    assert(similarity_model_create(weights, count, &model) == SIMILARITY_MODEL_OK);
    return model;
}

static void assert_result(const CorpusSearchResults *results, size_t index, const char *piece, size_t first,
                          size_t last, unsigned long numerator, unsigned long denominator) {
    assert(index < results->count);
    const SearchResult *result = results->results[index];
    char first_id[64], last_id[64];
    snprintf(first_id, sizeof(first_id), "%s-%zu", piece, first);
    snprintf(last_id, sizeof(last_id), "%s-%zu", piece, last);
    assert(strcmp(search_result_piece_ref(result), piece) == 0);
    assert(strcmp(search_result_matched_object_id(result), first_id) == 0);
    assert(strcmp(search_result_region_start_id(result), first_id) == 0);
    assert(strcmp(search_result_region_end_id(result), last_id) == 0);
    assert(search_result_similarity_numerator(result) == numerator);
    assert(search_result_similarity_denominator(result) == denominator);
}

/* piece-a: 2 2 -3 0 2 2 -3 7.  piece-b: 2 2 -3 0 5. Query: 2 2 -3 0. Interval only. */
static void test_ordering_ties_and_threshold(void) {
    Store store = open_store("test_corpus_search_ordering.sqlite");
    const long a[] = {2, 2, -3, 0, 2, 2, -3, 7};
    const long b[] = {2, 2, -3, 0, 5};
    add_piece(&store, "piece-a", a, 8);
    add_piece(&store, "piece-b", b, 5);
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "no-such-piece") == CORPUS_API_OK);

    const long query_intervals[] = {2, 2, -3, 0};
    SimilarityInput query = {query_intervals, 4, NULL, 0, NULL, 0};
    SimilarityDimensionWeight interval[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    SimilarityModel *model = make_model(interval, 1);

    /* Exact matches only: the two places it occurs, in piece order, at similarity 1. */
    CorpusSearchResults results = {NULL, 0};
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 1, 100, &results) ==
           CORPUS_SEARCH_OK);
    assert(results.count == 2);
    assert_result(&results, 0, "piece-a", 0, 3, 1, 1);
    assert_result(&results, 1, "piece-b", 0, 3, 1, 1);

    /* Each result states its criteria and evidence, and refers to what it came from. */
    const SearchResult *first = results.results[0];
    assert(search_result_criterion_count(first) == 1);
    assert(search_result_criterion_at(first, 0)->dimension == SIMILARITY_DIMENSION_INTERVAL);
    assert(search_result_criterion_at(first, 0)->weight_numerator == 1 && search_result_criterion_at(first, 0)->weight_denominator == 1);
    assert(search_result_evidence_count(first) == 1);
    assert(search_result_evidence_at(first, 0)->score_numerator == 1 && search_result_evidence_at(first, 0)->score_denominator == 1);
    assert(provenance_reference_state(search_result_provenance(first)) == PROVENANCE_REFERENCE_PRESENT);
    assert(strcmp(entity_id_value(provenance_reference_target_id(search_result_provenance(first))), "piece-a-0") == 0);
    corpus_search_results_free(&results);
    assert(results.results == NULL && results.count == 0);

    /* Lower the minimum to 3/4: the near match after the exact ones. Best first, then piece order, then start. */
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 3, 4, 100, &results) ==
           CORPUS_SEARCH_OK);
    assert(results.count == 3);
    assert_result(&results, 0, "piece-a", 0, 3, 1, 1);
    assert_result(&results, 1, "piece-b", 0, 3, 1, 1);
    assert_result(&results, 2, "piece-a", 4, 7, 3, 4);
    corpus_search_results_free(&results);

    /* To 1/4: three windows tie at 1/4 and keep piece order, then start (a1, a3, b1). */
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 4, 100, &results) ==
           CORPUS_SEARCH_OK);
    assert(results.count == 6);
    assert_result(&results, 3, "piece-a", 1, 4, 1, 4);
    assert_result(&results, 4, "piece-a", 3, 6, 1, 4);
    assert_result(&results, 5, "piece-b", 1, 4, 1, 4);
    corpus_search_results_free(&results);

    /* max_results keeps the best, in order; the minimum is exact (2/8 is 1/4, so the 1/4 windows are in). */
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 2, 8, 3, &results) ==
           CORPUS_SEARCH_OK);
    assert(results.count == 3);
    assert_result(&results, 0, "piece-a", 0, 3, 1, 1);
    assert_result(&results, 1, "piece-b", 0, 3, 1, 1);
    assert_result(&results, 2, "piece-a", 4, 7, 3, 4);
    corpus_search_results_free(&results);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 1, 1, &results) ==
           CORPUS_SEARCH_OK);
    assert(results.count == 1);
    assert_result(&results, 0, "piece-a", 0, 3, 1, 1);
    corpus_search_results_free(&results);

    /* The same search twice gives the same results in the same order. */
    CorpusSearchResults again = {NULL, 0};
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 4, 100, &results) == CORPUS_SEARCH_OK);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 4, 100, &again) == CORPUS_SEARCH_OK);
    assert(results.count == again.count);
    for (size_t i = 0; i < results.count; i++) {
        assert(strcmp(search_result_piece_ref(results.results[i]), search_result_piece_ref(again.results[i])) == 0);
        assert(strcmp(search_result_matched_object_id(results.results[i]), search_result_matched_object_id(again.results[i])) == 0);
        assert(search_result_similarity_numerator(results.results[i]) == search_result_similarity_numerator(again.results[i]));
    }
    corpus_search_results_free(&results);
    corpus_search_results_free(&again);

    /* A minimum above every score is no data, and the output is left alone. */
    const long unlike_intervals[] = {9, 9, 9, 9};
    SimilarityInput unlike = {unlike_intervals, 4, NULL, 0, NULL, 0};
    results.count = 77;
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &unlike, model, 1, 100, 100, &results) ==
           CORPUS_SEARCH_NO_DATA);
    assert(results.count == 77);
    /* ... also when the query is longer than any piece, or the corpus is unknown. */
    const long long_intervals[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    SimilarityInput long_query = {long_intervals, 10, NULL, 0, NULL, 0};
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &long_query, model, 0, 1, 100, &results) ==
           CORPUS_SEARCH_NO_DATA);
    assert(corpus_search_melodic(store.corpus, store.persistence, "no-such-corpus", &query, model, 0, 1, 100, &results) ==
           CORPUS_SEARCH_NO_DATA);
    assert(results.count == 77);

    similarity_model_destroy(model);
    close_store(&store);
}

/* Two single-window pieces, worked by hand. Query 2 2 -3 0.
 *   piece-x: 1 1 -1 0   interval 1/4, contour 1   (up up down same, as the query's)
 *   piece-y: 2 2 3 0    interval 3/4, contour 3/4 (up up up same)
 */
static void test_weights_change_the_ranking(void) {
    Store store = open_store("test_corpus_search_weights.sqlite");
    const long x[] = {1, 1, -1, 0};
    const long y[] = {2, 2, 3, 0};
    add_piece(&store, "piece-x", x, 4);
    add_piece(&store, "piece-y", y, 4);
    const long query_intervals[] = {2, 2, -3, 0};
    SimilarityInput query = {query_intervals, 4, NULL, 0, NULL, 0};
    CorpusSearchResults results = {NULL, 0};

    /* Interval alone: y (3/4) before x (1/4). */
    SimilarityDimensionWeight interval_only[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    SimilarityModel *model = make_model(interval_only, 1);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_OK);
    assert(results.count == 2);
    assert_result(&results, 0, "piece-y", 0, 3, 3, 4);
    assert_result(&results, 1, "piece-x", 0, 3, 1, 4);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);

    /* Contour alone: x (1) before y (3/4). */
    SimilarityDimensionWeight contour_only[] = {{SIMILARITY_DIMENSION_CONTOUR, 1, 1}};
    model = make_model(contour_only, 1);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_OK);
    assert_result(&results, 0, "piece-x", 0, 3, 1, 1);
    assert_result(&results, 1, "piece-y", 0, 3, 3, 4);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);

    /* Equal weights: x = (1/4 + 1) / 2 = 5/8, y = (3/4 + 3/4) / 2 = 3/4, so y first. */
    SimilarityDimensionWeight equal[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_CONTOUR, 1, 1}};
    model = make_model(equal, 2);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_OK);
    assert_result(&results, 0, "piece-y", 0, 3, 3, 4);
    assert_result(&results, 1, "piece-x", 0, 3, 5, 8);
    /* The evidence is each dimension's own score, and the criteria are the weights used. */
    const SearchResult *worse = results.results[1];
    assert(search_result_evidence_count(worse) == 2);
    assert(search_result_evidence_at(worse, 0)->dimension == SIMILARITY_DIMENSION_INTERVAL);
    assert(search_result_evidence_at(worse, 0)->score_numerator == 1 && search_result_evidence_at(worse, 0)->score_denominator == 4);
    assert(search_result_evidence_at(worse, 1)->dimension == SIMILARITY_DIMENSION_CONTOUR);
    assert(search_result_evidence_at(worse, 1)->score_numerator == 1 && search_result_evidence_at(worse, 1)->score_denominator == 1);
    assert(search_result_criterion_count(worse) == 2);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);

    /* Contour weighted three to one: x = (1/4 + 3) / 4 = 13/16, y = (3/4 + 9/4) / 4 = 3/4 = 12/16, so x first. */
    SimilarityDimensionWeight contour_heavy[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_CONTOUR, 3, 1}};
    model = make_model(contour_heavy, 2);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_OK);
    assert_result(&results, 0, "piece-x", 0, 3, 13, 16);
    assert_result(&results, 1, "piece-y", 0, 3, 3, 4);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);

    /* A zero-weight dimension is not used, so it is not in the criteria or the evidence. */
    SimilarityDimensionWeight zero_contour[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_CONTOUR, 0, 1}};
    model = make_model(zero_contour, 2);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_OK);
    assert(search_result_criterion_count(results.results[0]) == 1);
    assert(search_result_evidence_count(results.results[0]) == 1);
    assert_result(&results, 0, "piece-y", 0, 3, 3, 4);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);
    close_store(&store);
}

static void test_contour_query_and_transposition(void) {
    Store store = open_store("test_corpus_search_contour.sqlite");
    const long original[] = {2, 2, -3, 0};
    const long other_notes_same_shape[] = {5, 4, -9, 0}; /* up up down same, with other intervals */
    add_piece(&store, "piece-o", original, 4);
    add_piece(&store, "piece-s", other_notes_same_shape, 4);

    /* A query by direction alone, on the contour dimension: both have the shape. */
    const ContourDirection shape[] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_SAME};
    SimilarityInput by_direction = {NULL, 0, shape, 4, NULL, 0};
    SimilarityDimensionWeight contour_only[] = {{SIMILARITY_DIMENSION_CONTOUR, 1, 1}};
    SimilarityModel *model = make_model(contour_only, 1);
    CorpusSearchResults results = {NULL, 0};
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &by_direction, model, 1, 1, 10, &results) ==
           CORPUS_SEARCH_OK);
    assert(results.count == 2);
    assert_result(&results, 0, "piece-o", 0, 3, 1, 1);
    assert_result(&results, 1, "piece-s", 0, 3, 1, 1);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);

    /* The interval dimension needs intervals in the query. */
    SimilarityDimensionWeight interval_only[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    model = make_model(interval_only, 1);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &by_direction, model, 0, 1, 10, &results) ==
           CORPUS_SEARCH_ERR_INVALID_ARG);
    similarity_model_destroy(model);
    close_store(&store);
}

static void test_rejections(void) {
    Store store = open_store("test_corpus_search_rejections.sqlite");
    const long melody[] = {2, 2, -3, 0};
    add_piece(&store, "piece-a", melody, 4);
    SimilarityInput query = {melody, 4, NULL, 0, NULL, 0};
    CorpusSearchResults results = {(SearchResult **)(intptr_t)0x1, 5};
    SimilarityDimensionWeight interval[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    SimilarityModel *model = make_model(interval, 1);

    /* No persisted durations: a model that counts rhythm cannot be searched, and nothing is invented to score it. */
    SimilarityDimensionWeight with_rhythm[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_RHYTHM, 1, 2}};
    SimilarityModel *rhythm_model = make_model(with_rhythm, 2);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, rhythm_model, 0, 1, 10, &results) ==
           CORPUS_SEARCH_ERR_DIMENSION_UNAVAILABLE);
    /* Declared with weight 0 it is not counted, so the search can run. */
    SimilarityDimensionWeight zero_rhythm[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_RHYTHM, 0, 1}};
    SimilarityModel *zero_model = make_model(zero_rhythm, 2);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, zero_model, 1, 1, 10, &results) ==
           CORPUS_SEARCH_OK);
    corpus_search_results_free(&results);
    results.results = (SearchResult **)(intptr_t)0x1;
    results.count = 5;

    /* Arguments out of range or missing; the output is left alone each time. */
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 1, 0, &results) == CORPUS_SEARCH_ERR_INVALID_ARG);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 1, 0, 10, &results) == CORPUS_SEARCH_ERR_INVALID_ARG);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 3, 2, 10, &results) == CORPUS_SEARCH_ERR_INVALID_ARG);
    SimilarityInput empty_query = {NULL, 0, NULL, 0, NULL, 0};
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &empty_query, model, 0, 1, 10, &results) == CORPUS_SEARCH_ERR_INVALID_ARG);
    assert(corpus_search_melodic(NULL, store.persistence, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_ERR_NULL_ARG);
    assert(corpus_search_melodic(store.corpus, NULL, "corpus-1", &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_ERR_NULL_ARG);
    assert(corpus_search_melodic(store.corpus, store.persistence, NULL, &query, model, 0, 1, 10, &results) == CORPUS_SEARCH_ERR_NULL_ARG);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", NULL, model, 0, 1, 10, &results) == CORPUS_SEARCH_ERR_NULL_ARG);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, NULL, 0, 1, 10, &results) == CORPUS_SEARCH_ERR_NULL_ARG);
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 10, NULL) == CORPUS_SEARCH_ERR_NULL_ARG);
    assert(results.results == (SearchResult **)(intptr_t)0x1 && results.count == 5);
    corpus_search_results_free(NULL);

    similarity_model_destroy(model);
    similarity_model_destroy(rhythm_model);
    similarity_model_destroy(zero_model);
    close_store(&store);
}

static void test_a_claim_that_does_not_decode_breaks_its_windows(void) {
    Store store = open_store("test_corpus_search_bad_claim.sqlite");
    /* piece-bad: 2 2 [bad] -3 0 2 2: only windows that avoid the bad claim count. */
    const long first_half[] = {2, 2};
    char id[64];
    for (size_t i = 0; i < 2; i++) {
        snprintf(id, sizeof(id), "piece-bad-%zu", i);
        stats_store_claim(store.persistence, id, CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, &first_half[i], sizeof(long), "piece-bad");
    }
    stats_store_claim(store.persistence, "piece-bad-2", CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, "ab", 2, "piece-bad");
    const long second_half[] = {-3, 0, 2, 2};
    for (size_t i = 0; i < 4; i++) {
        snprintf(id, sizeof(id), "piece-bad-%zu", i + 3);
        stats_store_claim(store.persistence, id, CORPUS_SEARCH_INTERVAL_CLAIM_TYPE, &second_half[i], sizeof(long), "piece-bad");
    }
    assert(corpus_api_add_piece_ref(store.corpus, "corpus-1", "piece-bad") == CORPUS_API_OK);

    const long query_intervals[] = {-3, 0, 2, 2};
    SimilarityInput query = {query_intervals, 4, NULL, 0, NULL, 0};
    SimilarityDimensionWeight interval[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    SimilarityModel *model = make_model(interval, 1);
    CorpusSearchResults results = {NULL, 0};
    assert(corpus_search_melodic(store.corpus, store.persistence, "corpus-1", &query, model, 0, 1, 100, &results) == CORPUS_SEARCH_OK);
    /* Windows start at 0..3 over 7 claims; those covering index 2 (starts 0 to 2) are broken; start 3 is whole. */
    assert(results.count == 1);
    assert_result(&results, 0, "piece-bad", 3, 6, 1, 1);
    corpus_search_results_free(&results);
    similarity_model_destroy(model);
    close_store(&store);
}

int main(void) {
    test_ordering_ties_and_threshold();
    test_weights_change_the_ranking();
    test_contour_query_and_transposition();
    test_rejections();
    test_a_claim_that_does_not_decode_breaks_its_windows();
    printf("test_corpus_search: ok\n");
    return 0;
}
