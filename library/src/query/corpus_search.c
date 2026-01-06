#include "query/corpus_search.h"

#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "persistence/persistence_context_query.h"
#include "query/query_fraction.h"

/* A window that met the minimum, kept until the best are chosen. */
typedef struct {
    size_t piece_index;
    size_t start;
    QueryFraction similarity;
    SimilarityBreakdown breakdown;
    char *first_id;
    char *last_id;
} Match;

typedef struct {
    Match *items;
    size_t count;
    size_t capacity;
} Matches;

typedef struct {
    const SimilarityInput *query;
    const SimilarityModel *model;
    QueryFraction minimum;
    size_t window;
    Matches *matches;
    CorpusSearchResult status;
} Walk;

static void free_match(Match *match) {
    free(match->first_id);
    free(match->last_id);
}

static void free_matches(Matches *matches) {
    for (size_t i = 0; i < matches->count; i++) {
        free_match(&matches->items[i]);
    }
    free(matches->items);
    matches->items = NULL;
    matches->count = 0;
}

static char *copy_string(const char *text) {
    size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

static CorpusSearchResult add_match(Matches *matches, size_t piece_index, size_t start, QueryFraction similarity,
                                    const SimilarityBreakdown *breakdown, const char *first_id, const char *last_id) {
    if (matches->count == matches->capacity) {
        size_t capacity = matches->capacity == 0 ? 16 : matches->capacity * 2;
        Match *grown = (Match *)realloc(matches->items, capacity * sizeof(*grown));
        if (grown == NULL) {
            return CORPUS_SEARCH_ERR_ALLOC;
        }
        matches->items = grown;
        matches->capacity = capacity;
    }
    Match *match = &matches->items[matches->count];
    match->piece_index = piece_index;
    match->start = start;
    match->similarity = similarity;
    match->breakdown = *breakdown;
    match->first_id = copy_string(first_id);
    match->last_id = copy_string(last_id);
    if (match->first_id == NULL || match->last_id == NULL) {
        free_match(match);
        return CORPUS_SEARCH_ERR_ALLOC;
    }
    matches->count++;
    return CORPUS_SEARCH_OK;
}

/* Scores every window of one piece's persisted intervals. */
static CorpusSearchResult search_piece(PersistenceHandle *persistence_handle, const char *piece_ref, size_t piece_index,
                                       Walk *walk) {
    char **ids = NULL;
    size_t id_count = 0;
    PersistenceClaimContent *contents = NULL;
    size_t content_count = 0;
    if (persistence_list_interpretation_ids_for_context(persistence_handle, piece_ref, CORPUS_SEARCH_INTERVAL_CLAIM_TYPE,
                                                        &ids, &id_count) != PERSISTENCE_QUERY_OK) {
        return CORPUS_SEARCH_OK;
    }
    if (persistence_list_claim_contents_for_context(persistence_handle, piece_ref, CORPUS_SEARCH_INTERVAL_CLAIM_TYPE,
                                                    &contents, &content_count) != PERSISTENCE_QUERY_OK) {
        persistence_free_id_list(ids, id_count);
        return CORPUS_SEARCH_OK;
    }

    CorpusSearchResult result = CORPUS_SEARCH_OK;
    size_t n = walk->window;
    if (id_count == content_count && id_count >= n) {
        long *intervals = (long *)malloc(id_count * sizeof(long));
        unsigned char *valid = (unsigned char *)calloc(id_count, 1);
        if (intervals == NULL || valid == NULL) {
            result = CORPUS_SEARCH_ERR_ALLOC;
        } else {
            for (size_t i = 0; i < id_count; i++) {
                if (contents[i].size == sizeof(long)) {
                    memcpy(&intervals[i], contents[i].bytes, sizeof(long));
                    valid[i] = 1;
                }
            }
            for (size_t start = 0; start + n <= id_count && result == CORPUS_SEARCH_OK; start++) {
                int complete = 1;
                for (size_t i = 0; i < n; i++) {
                    if (!valid[start + i]) {
                        complete = 0;
                        break;
                    }
                }
                if (!complete) {
                    continue;
                }
                SimilarityInput candidate = {&intervals[start], n, NULL, 0, NULL, 0};
                SimilarityBreakdown breakdown;
                SimilarityScoreResult scored = similarity_score(walk->model, walk->query, &candidate, &breakdown);
                if (scored == SIMILARITY_SCORE_ERR_OVERFLOW) {
                    result = CORPUS_SEARCH_ERR_OVERFLOW;
                    break;
                }
                if (scored == SIMILARITY_SCORE_ERR_ALLOC) {
                    result = CORPUS_SEARCH_ERR_ALLOC;
                    break;
                }
                if (scored != SIMILARITY_SCORE_OK) {
                    result = CORPUS_SEARCH_ERR_INVALID_ARG; /* query data a counted dimension needs is missing */
                    break;
                }
                QueryFraction similarity = {breakdown.similarity.numerator, breakdown.similarity.denominator};
                int comparison = 0;
                if (query_fraction_compare(similarity, walk->minimum, &comparison) != QUERY_FRACTION_OK) {
                    result = CORPUS_SEARCH_ERR_OVERFLOW;
                    break;
                }
                if (comparison >= 0) {
                    result = add_match(walk->matches, piece_index, start, similarity, &breakdown, ids[start],
                                       ids[start + n - 1]);
                }
            }
        }
        free(intervals);
        free(valid);
    }

    persistence_free_claim_contents(contents, content_count);
    persistence_free_id_list(ids, id_count);
    return result;
}

/* Best first; equal similarity keeps piece order, then window start. A total order, so any sort is deterministic. */
static int compare_matches(const void *left, const void *right) {
    const Match *a = (const Match *)left;
    const Match *b = (const Match *)right;
    int comparison = 0;
    query_fraction_compare(a->similarity, b->similarity, &comparison);
    if (comparison != 0) {
        return -comparison;
    }
    if (a->piece_index != b->piece_index) {
        return a->piece_index < b->piece_index ? -1 : 1;
    }
    if (a->start != b->start) {
        return a->start < b->start ? -1 : 1;
    }
    return 0;
}

static CorpusSearchResult build_result(const Match *match, const char *piece_ref, const SimilarityModel *model,
                                       SearchResult **out) {
    SearchResultCriterion criteria[SIMILARITY_DIMENSION_COUNT];
    SearchResultEvidence evidence[SIMILARITY_DIMENSION_COUNT];
    size_t criterion_count = 0;
    for (size_t i = 0; i < similarity_model_dimension_count(model); i++) {
        SimilarityDimension dimension;
        unsigned long numerator, denominator;
        similarity_model_dimension_at(model, i, &dimension, &numerator, &denominator);
        if (numerator == 0) {
            continue; /* weight 0: not used */
        }
        criteria[criterion_count].dimension = dimension;
        criteria[criterion_count].weight_numerator = numerator;
        criteria[criterion_count].weight_denominator = denominator;
        criterion_count++;
    }
    for (size_t i = 0; i < match->breakdown.dimension_score_count; i++) {
        evidence[i].dimension = match->breakdown.dimension_scores[i].dimension;
        evidence[i].score_numerator = match->breakdown.dimension_scores[i].score.numerator;
        evidence[i].score_denominator = match->breakdown.dimension_scores[i].score.denominator;
    }

    EntityID *target = NULL;
    ProvenanceReference *provenance = NULL;
    if (entity_id_create(match->first_id, &target) != ENTITY_ID_OK) {
        return CORPUS_SEARCH_ERR_ALLOC;
    }
    if (provenance_reference_create_present(target, &provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(target);
        return CORPUS_SEARCH_ERR_ALLOC;
    }
    SearchResultResult created =
        search_result_create(piece_ref, match->first_id, match->first_id, match->last_id, match->similarity.numerator,
                             match->similarity.denominator, criteria, criterion_count, evidence,
                             match->breakdown.dimension_score_count, provenance, out);
    if (created != SEARCH_RESULT_OK) {
        provenance_reference_destroy(provenance);
        return CORPUS_SEARCH_ERR_ALLOC;
    }
    return CORPUS_SEARCH_OK;
}

void corpus_search_results_free(CorpusSearchResults *results) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < results->count; i++) {
        search_result_destroy(results->results[i]);
    }
    free(results->results);
    results->results = NULL;
    results->count = 0;
}

CorpusSearchResult corpus_search_melodic(CorpusStoreHandle *corpus_handle, PersistenceHandle *persistence_handle,
                                         const char *corpus_id, const SimilarityInput *query,
                                         const SimilarityModel *model, unsigned long minimum_numerator,
                                         unsigned long minimum_denominator, size_t max_results,
                                         CorpusSearchResults *out) {
    if (corpus_handle == NULL || persistence_handle == NULL || corpus_id == NULL || query == NULL || model == NULL ||
        out == NULL) {
        return CORPUS_SEARCH_ERR_NULL_ARG;
    }
    if (max_results == 0 || minimum_denominator == 0 || minimum_numerator > minimum_denominator) {
        return CORPUS_SEARCH_ERR_INVALID_ARG;
    }
    unsigned long rhythm_numerator = 0;
    if (similarity_model_weight(model, SIMILARITY_DIMENSION_RHYTHM, &rhythm_numerator, NULL) && rhythm_numerator > 0) {
        return CORPUS_SEARCH_ERR_DIMENSION_UNAVAILABLE;
    }
    size_t window = query->interval_count > 0 ? query->interval_count : query->direction_count;
    if (window == 0) {
        return CORPUS_SEARCH_ERR_INVALID_ARG;
    }
    QueryFraction minimum;
    if (query_fraction_reduce((QueryFraction){minimum_numerator, minimum_denominator}, &minimum) !=
        QUERY_FRACTION_OK) {
        return CORPUS_SEARCH_ERR_INVALID_ARG;
    }

    char **refs = NULL;
    size_t ref_count = 0;
    if (corpus_api_list_piece_refs(corpus_handle, corpus_id, &refs, &ref_count) != CORPUS_API_OK) {
        return CORPUS_SEARCH_ERR_DB;
    }

    Matches matches = {NULL, 0, 0};
    Walk walk = {query, model, minimum, window, &matches, CORPUS_SEARCH_OK};
    CorpusSearchResult result = CORPUS_SEARCH_OK;
    for (size_t i = 0; i < ref_count && result == CORPUS_SEARCH_OK; i++) {
        result = search_piece(persistence_handle, refs[i], i, &walk);
    }

    CorpusSearchResults found = {NULL, 0};
    if (result == CORPUS_SEARCH_OK) {
        if (matches.count == 0) {
            result = CORPUS_SEARCH_NO_DATA;
        } else {
            qsort(matches.items, matches.count, sizeof(Match), compare_matches);
            size_t keep = matches.count < max_results ? matches.count : max_results;
            found.results = (SearchResult **)calloc(keep, sizeof(SearchResult *));
            if (found.results == NULL) {
                result = CORPUS_SEARCH_ERR_ALLOC;
            }
            for (size_t i = 0; i < keep && result == CORPUS_SEARCH_OK; i++) {
                result = build_result(&matches.items[i], refs[matches.items[i].piece_index], model, &found.results[i]);
                if (result == CORPUS_SEARCH_OK) {
                    found.count++;
                }
            }
        }
    }

    free_matches(&matches);
    for (size_t i = 0; i < ref_count; i++) {
        free(refs[i]);
    }
    free(refs);

    if (result != CORPUS_SEARCH_OK) {
        corpus_search_results_free(&found);
        return result;
    }
    *out = found;
    return CORPUS_SEARCH_OK;
}
