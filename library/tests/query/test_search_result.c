#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "query/search_result.h"

/*
 * A SearchResult owns what it is given (strings and arrays are copied, the
 * provenance is taken), reduces its fractions, reads back exactly, and is
 * refused when a field it needs is empty or out of range.
 */

static ProvenanceReference *make_provenance(const char *target) {
    EntityID *id = NULL;
    assert(entity_id_create(target, &id) == ENTITY_ID_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_present(id, &provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static const SearchResultCriterion CRITERIA[] = {{SIMILARITY_DIMENSION_INTERVAL, 2, 4}, {SIMILARITY_DIMENSION_CONTOUR, 1, 1}};
static const SearchResultEvidence EVIDENCE[] = {{SIMILARITY_DIMENSION_INTERVAL, 2, 4}, {SIMILARITY_DIMENSION_CONTOUR, 3, 3}};

static void test_create_and_read_back(void) {
    char piece[] = "Mozart 545.mid";
    char matched[] = "interpretation-17";
    char start[] = "interpretation-17";
    char end[] = "interpretation-20";
    SearchResultCriterion criteria[2] = {CRITERIA[0], CRITERIA[1]};
    SearchResultEvidence evidence[2] = {EVIDENCE[0], EVIDENCE[1]};
    ProvenanceReference *provenance = make_provenance("interpretation-17");

    SearchResult *result = NULL;
    assert(search_result_create(piece, matched, start, end, 6, 8, criteria, 2, evidence, 2, provenance, &result) ==
           SEARCH_RESULT_OK);

    /* What was passed in can change or go away: the result owns copies. */
    memset(piece, 'x', sizeof(piece) - 1);
    memset(end, 'y', sizeof(end) - 1);
    criteria[0].dimension = SIMILARITY_DIMENSION_RHYTHM;
    evidence[1].score_numerator = 0;

    assert(strcmp(search_result_piece_ref(result), "Mozart 545.mid") == 0);
    assert(strcmp(search_result_matched_object_id(result), "interpretation-17") == 0);
    assert(strcmp(search_result_region_start_id(result), "interpretation-17") == 0);
    assert(strcmp(search_result_region_end_id(result), "interpretation-20") == 0);

    /* The similarity and every fraction are in lowest terms. */
    assert(search_result_similarity_numerator(result) == 3 && search_result_similarity_denominator(result) == 4);
    assert(search_result_criterion_count(result) == 2);
    const SearchResultCriterion *first = search_result_criterion_at(result, 0);
    assert(first->dimension == SIMILARITY_DIMENSION_INTERVAL && first->weight_numerator == 1 && first->weight_denominator == 2);
    const SearchResultCriterion *second = search_result_criterion_at(result, 1);
    assert(second->dimension == SIMILARITY_DIMENSION_CONTOUR && second->weight_numerator == 1 && second->weight_denominator == 1);
    assert(search_result_criterion_at(result, 2) == NULL);
    assert(search_result_evidence_count(result) == 2);
    const SearchResultEvidence *score = search_result_evidence_at(result, 0);
    assert(score->dimension == SIMILARITY_DIMENSION_INTERVAL && score->score_numerator == 1 && score->score_denominator == 2);
    score = search_result_evidence_at(result, 1);
    assert(score->dimension == SIMILARITY_DIMENSION_CONTOUR && score->score_numerator == 1 && score->score_denominator == 1);
    assert(search_result_evidence_at(result, 2) == NULL);

    /* The provenance is the one given, taken over. */
    assert(search_result_provenance(result) == provenance);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    search_result_destroy(result);
    search_result_destroy(NULL);
}

static SearchResultResult create_with(const char *piece, const char *matched, const char *start, const char *end,
                                      unsigned long numerator, unsigned long denominator,
                                      const SearchResultCriterion *criteria, size_t criterion_count,
                                      const SearchResultEvidence *evidence, size_t evidence_count, int with_provenance) {
    ProvenanceReference *provenance = with_provenance ? make_provenance("p") : NULL;
    SearchResult *result = (SearchResult *)0x1;
    SearchResultResult status = search_result_create(piece, matched, start, end, numerator, denominator, criteria,
                                                     criterion_count, evidence, evidence_count, provenance, &result);
    if (status == SEARCH_RESULT_OK) {
        search_result_destroy(result);
    } else {
        assert(result == (SearchResult *)0x1); /* untouched on any error */
        provenance_reference_destroy(provenance); /* not taken on error */
    }
    return status;
}

static void test_invalid_results(void) {
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_OK);

    /* Missing and empty fields. */
    assert(create_with(NULL, "m", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_NULL_ARGUMENT);
    assert(create_with("p", NULL, "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_NULL_ARGUMENT);
    assert(create_with("p", "m", NULL, "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_NULL_ARGUMENT);
    assert(create_with("p", "m", "s", NULL, 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_NULL_ARGUMENT);
    assert(create_with("p", "m", "s", "e", 1, 2, NULL, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_NULL_ARGUMENT);
    assert(create_with("", "m", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_EMPTY_FIELD);
    assert(create_with("p", "", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_EMPTY_FIELD);
    assert(create_with("p", "m", "", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_EMPTY_FIELD);
    assert(create_with("p", "m", "s", "", 1, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_EMPTY_FIELD);
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 0, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_EMPTY_FIELD);
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 0, 1) == SEARCH_RESULT_ERR_EMPTY_FIELD);

    /* A similarity must be a fraction in [0, 1]. */
    assert(create_with("p", "m", "s", "e", 1, 0, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_INVALID_SIMILARITY);
    assert(create_with("p", "m", "s", "e", 3, 2, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_INVALID_SIMILARITY);
    assert(create_with("p", "m", "s", "e", 0, 5, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_OK);
    assert(create_with("p", "m", "s", "e", 5, 5, CRITERIA, 2, EVIDENCE, 2, 1) == SEARCH_RESULT_OK);

    /* Criteria and evidence must name a dimension and carry exact fractions. */
    const SearchResultCriterion bad_dimension[] = {{(SimilarityDimension)9, 1, 1}};
    const SearchResultCriterion bad_weight[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 0}};
    assert(create_with("p", "m", "s", "e", 1, 2, bad_dimension, 1, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_INVALID_CRITERION);
    assert(create_with("p", "m", "s", "e", 1, 2, bad_weight, 1, EVIDENCE, 2, 1) == SEARCH_RESULT_ERR_INVALID_CRITERION);
    const SearchResultEvidence bad_evidence_dimension[] = {{(SimilarityDimension)9, 1, 1}};
    const SearchResultEvidence bad_denominator[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 0}};
    const SearchResultEvidence over_one[] = {{SIMILARITY_DIMENSION_INTERVAL, 3, 2}};
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 2, bad_evidence_dimension, 1, 1) == SEARCH_RESULT_ERR_INVALID_EVIDENCE);
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 2, bad_denominator, 1, 1) == SEARCH_RESULT_ERR_INVALID_EVIDENCE);
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 2, over_one, 1, 1) == SEARCH_RESULT_ERR_INVALID_EVIDENCE);

    /* Provenance is required. */
    assert(create_with("p", "m", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, 0) == SEARCH_RESULT_ERR_NULL_PROVENANCE);
    assert(search_result_create("p", "m", "s", "e", 1, 2, CRITERIA, 2, EVIDENCE, 2, NULL, NULL) ==
           SEARCH_RESULT_ERR_NULL_ARGUMENT);
}

int main(void) {
    test_create_and_read_back();
    test_invalid_results();
    printf("test_search_result: ok\n");
    return 0;
}
