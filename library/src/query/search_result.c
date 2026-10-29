#include "query/search_result.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"
#include "query/query_fraction.h"

struct SearchResult {
    char *piece_ref;
    char *matched_object_id;
    char *region_start_id;
    char *region_end_id;
    QueryFraction similarity;
    SearchResultCriterion *criteria;
    size_t criterion_count;
    SearchResultEvidence *evidence;
    size_t evidence_count;
    ProvenanceReference *provenance;
};

static int known_dimension(SimilarityDimension dimension) {
    return (int)dimension >= 0 && dimension < SIMILARITY_DIMENSION_COUNT;
}

static void free_fields(SearchResult *result) {
    free(result->piece_ref);
    free(result->matched_object_id);
    free(result->region_start_id);
    free(result->region_end_id);
    free(result->criteria);
    free(result->evidence);
}

SearchResultResult search_result_create(const char *piece_ref, const char *matched_object_id,
                                        const char *region_start_id, const char *region_end_id,
                                        unsigned long similarity_numerator, unsigned long similarity_denominator,
                                        const SearchResultCriterion *criteria, size_t criterion_count,
                                        const SearchResultEvidence *evidence, size_t evidence_count,
                                        ProvenanceReference *provenance, SearchResult **out) {
    if (piece_ref == NULL || matched_object_id == NULL || region_start_id == NULL || region_end_id == NULL ||
        out == NULL || (criterion_count > 0 && criteria == NULL) || (evidence_count > 0 && evidence == NULL)) {
        return SEARCH_RESULT_ERR_NULL_ARGUMENT;
    }
    if (piece_ref[0] == '\0' || matched_object_id[0] == '\0' || region_start_id[0] == '\0' ||
        region_end_id[0] == '\0' || criterion_count == 0 || evidence_count == 0) {
        return SEARCH_RESULT_ERR_EMPTY_FIELD;
    }
    QueryFraction similarity = {similarity_numerator, similarity_denominator};
    if (similarity_denominator == 0 || similarity_numerator > similarity_denominator ||
        query_fraction_reduce(similarity, &similarity) != QUERY_FRACTION_OK) {
        return SEARCH_RESULT_ERR_INVALID_SIMILARITY;
    }
    for (size_t i = 0; i < criterion_count; i++) {
        if (!known_dimension(criteria[i].dimension) || criteria[i].weight_denominator == 0) {
            return SEARCH_RESULT_ERR_INVALID_CRITERION;
        }
    }
    for (size_t i = 0; i < evidence_count; i++) {
        if (!known_dimension(evidence[i].dimension) || evidence[i].score_denominator == 0 ||
            evidence[i].score_numerator > evidence[i].score_denominator) {
            return SEARCH_RESULT_ERR_INVALID_EVIDENCE;
        }
    }
    if (provenance == NULL) {
        return SEARCH_RESULT_ERR_NULL_PROVENANCE;
    }

    SearchResult *result = (SearchResult *)calloc(1, sizeof(*result));
    if (result == NULL) {
        return SEARCH_RESULT_ERR_ALLOC;
    }
    result->piece_ref = ursatz_string_copy(piece_ref);
    result->matched_object_id = ursatz_string_copy(matched_object_id);
    result->region_start_id = ursatz_string_copy(region_start_id);
    result->region_end_id = ursatz_string_copy(region_end_id);
    result->criteria = (SearchResultCriterion *)malloc(criterion_count * sizeof(*result->criteria));
    result->evidence = (SearchResultEvidence *)malloc(evidence_count * sizeof(*result->evidence));
    if (result->piece_ref == NULL || result->matched_object_id == NULL || result->region_start_id == NULL ||
        result->region_end_id == NULL || result->criteria == NULL || result->evidence == NULL) {
        free_fields(result);
        free(result);
        return SEARCH_RESULT_ERR_ALLOC;
    }
    for (size_t i = 0; i < criterion_count; i++) {
        QueryFraction weight = {criteria[i].weight_numerator, criteria[i].weight_denominator};
        query_fraction_reduce(weight, &weight);
        result->criteria[i].dimension = criteria[i].dimension;
        result->criteria[i].weight_numerator = weight.numerator;
        result->criteria[i].weight_denominator = weight.denominator;
    }
    for (size_t i = 0; i < evidence_count; i++) {
        QueryFraction score = {evidence[i].score_numerator, evidence[i].score_denominator};
        query_fraction_reduce(score, &score);
        result->evidence[i].dimension = evidence[i].dimension;
        result->evidence[i].score_numerator = score.numerator;
        result->evidence[i].score_denominator = score.denominator;
    }
    result->criterion_count = criterion_count;
    result->evidence_count = evidence_count;
    result->similarity = similarity;
    result->provenance = provenance;
    *out = result;
    return SEARCH_RESULT_OK;
}

void search_result_destroy(SearchResult *result) {
    if (result == NULL) {
        return;
    }
    free_fields(result);
    provenance_reference_destroy(result->provenance);
    free(result);
}

const char *search_result_piece_ref(const SearchResult *result) {
    return result->piece_ref;
}

const char *search_result_matched_object_id(const SearchResult *result) {
    return result->matched_object_id;
}

const char *search_result_region_start_id(const SearchResult *result) {
    return result->region_start_id;
}

const char *search_result_region_end_id(const SearchResult *result) {
    return result->region_end_id;
}

unsigned long search_result_similarity_numerator(const SearchResult *result) {
    return result->similarity.numerator;
}

unsigned long search_result_similarity_denominator(const SearchResult *result) {
    return result->similarity.denominator;
}

size_t search_result_criterion_count(const SearchResult *result) {
    return result->criterion_count;
}

const SearchResultCriterion *search_result_criterion_at(const SearchResult *result, size_t index) {
    return index < result->criterion_count ? &result->criteria[index] : NULL;
}

size_t search_result_evidence_count(const SearchResult *result) {
    return result->evidence_count;
}

const SearchResultEvidence *search_result_evidence_at(const SearchResult *result, size_t index) {
    return index < result->evidence_count ? &result->evidence[index] : NULL;
}

const ProvenanceReference *search_result_provenance(const SearchResult *result) {
    return result->provenance;
}
