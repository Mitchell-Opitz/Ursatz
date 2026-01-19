#ifndef MUSIC_QUERY_SEARCH_RESULT_H
#define MUSIC_QUERY_SEARCH_RESULT_H

#include <stddef.h>

#include "core/provenance_reference.h"
#include "query/similarity_model.h"

/*
 * SearchResult (Value, music-query, Registry "Query"): one match a similarity
 * search found. It says where (target, match region), how alike (similarity)
 * and why that number (criteria, evidence), and what it came from
 * (provenance).
 *
 *   target        the piece ref the match is in, and the id of the matched
 *                 object: the id of the first persisted interpretation of the
 *                 matched window.
 *   match region  the ids of the first and last persisted interpretations of
 *                 the matched window. (Persisted interpretations carry no
 *                 event refs, so the region is stated in the ids analysis
 *                 persisted; both are the same id for a one-claim window.)
 *   similarity    the combined score, an exact rational in [0, 1] in lowest
 *                 terms (never float/double).
 *   criteria      the dimensions and weights the search was made with, each
 *                 weight an exact rational in lowest terms. Nothing is
 *                 compared on an undeclared dimension.
 *   evidence      each counted dimension's own score, which the similarity is
 *                 the weighted combination of.
 *   provenance    present, referring to the first matched interpretation.
 *
 * A SearchResult is a descriptive match, not a framework-validated claim: it
 * says the persisted data agrees on the stated criteria, never that the match
 * is musically significant, and it is not and cannot become a Rule.
 *
 * It owns its strings and arrays (all copied in, all freed by
 * search_result_destroy) and has no mutator.
 */

typedef struct SearchResult SearchResult;

typedef struct {
    SimilarityDimension dimension;
    unsigned long weight_numerator;
    unsigned long weight_denominator;
} SearchResultCriterion;

typedef struct {
    SimilarityDimension dimension;
    unsigned long score_numerator;
    unsigned long score_denominator;
} SearchResultEvidence;

typedef enum {
    SEARCH_RESULT_OK = 0,
    SEARCH_RESULT_ERR_NULL_ARGUMENT,
    SEARCH_RESULT_ERR_EMPTY_FIELD,        /* an empty string, or no criteria, or no evidence */
    SEARCH_RESULT_ERR_INVALID_SIMILARITY, /* a zero denominator, or a similarity above 1 */
    SEARCH_RESULT_ERR_INVALID_CRITERION,  /* an unknown dimension or a zero denominator */
    SEARCH_RESULT_ERR_INVALID_EVIDENCE,   /* an unknown dimension, zero denominator or score above 1 */
    SEARCH_RESULT_ERR_NULL_PROVENANCE,
    SEARCH_RESULT_ERR_ALLOC
} SearchResultResult;

/*
 * Copies the strings and arrays; takes ownership of provenance on success
 * only (destroyed by search_result_destroy). *out is untouched on any result
 * other than OK. similarity is reduced on the way in.
 */
SearchResultResult search_result_create(const char *piece_ref, const char *matched_object_id,
                                        const char *region_start_id, const char *region_end_id,
                                        unsigned long similarity_numerator, unsigned long similarity_denominator,
                                        const SearchResultCriterion *criteria, size_t criterion_count,
                                        const SearchResultEvidence *evidence, size_t evidence_count,
                                        ProvenanceReference *provenance, SearchResult **out);

/* Safe on NULL. */
void search_result_destroy(SearchResult *result);

const char *search_result_piece_ref(const SearchResult *result);
const char *search_result_matched_object_id(const SearchResult *result);
const char *search_result_region_start_id(const SearchResult *result);
const char *search_result_region_end_id(const SearchResult *result);
unsigned long search_result_similarity_numerator(const SearchResult *result);
unsigned long search_result_similarity_denominator(const SearchResult *result);
size_t search_result_criterion_count(const SearchResult *result);
/* NULL if index is out of range. */
const SearchResultCriterion *search_result_criterion_at(const SearchResult *result, size_t index);
size_t search_result_evidence_count(const SearchResult *result);
const SearchResultEvidence *search_result_evidence_at(const SearchResult *result, size_t index);
const ProvenanceReference *search_result_provenance(const SearchResult *result);

#endif
