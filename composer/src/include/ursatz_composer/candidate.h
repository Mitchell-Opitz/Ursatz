#ifndef URSATZ_COMPOSER_CANDIDATE_H
#define URSATZ_COMPOSER_CANDIDATE_H

#include <stdbool.h>
#include <stddef.h>

#include <core/provenance_reference.h>
#include <core/value.h>
#include <identity/entity_id.h>
#include <interpretation/interpretation.h>

/*
 * Candidate: a proposed result before it is accepted (TDD Sec158): material, evaluations, violations, score and
 * provenance. The fill loop's suggestion mode (fill_suggest.h) builds one per suggestion.
 *
 * material_id is a required EntityID reference to the proposed material, referenced by id only, never embedded.
 *
 * evaluations and violations are independent, owned arrays of Interpretation pointers with their counts, kept as two
 * separate fields. Both may be empty (count 0, pass NULL): a Candidate not yet evaluated is valid.
 *
 * score is an exact rational fraction (score_numerator / score_denominator, denominator > 0, never float/double).
 *
 * provenance is a required ProvenanceReference: what produced the Candidate.
 */

typedef struct Candidate Candidate;

typedef enum {
    CANDIDATE_OK = 0,
    CANDIDATE_ERR_NULL_ID,
    CANDIDATE_ERR_NULL_MATERIAL_ID,
    CANDIDATE_ERR_NULL_EVALUATION,
    CANDIDATE_ERR_NULL_VIOLATION,
    CANDIDATE_ERR_NON_POSITIVE_SCORE_DENOMINATOR,
    CANDIDATE_ERR_NULL_PROVENANCE,
    CANDIDATE_ERR_ALLOC
} CandidateResult;

/*
 * Takes ownership of id, material_id, each Interpretation in
 * evaluations (if any), each Interpretation in violations (if any), and
 * provenance on success (all destroyed by candidate_destroy).
 * evaluations/violations are optional -- pass NULL with the matching
 * count == 0 for either not supplied. No entry of a non-empty array may
 * be NULL. score_denominator must be > 0.
 */
CandidateResult candidate_create(EntityID *id, EntityID *material_id, Interpretation **evaluations,
                                  size_t evaluation_count, Interpretation **violations, size_t violation_count,
                                  unsigned long score_numerator, unsigned long score_denominator,
                                  ProvenanceReference *provenance, Candidate **out);
void candidate_destroy(Candidate *candidate);

unsigned long candidate_score_numerator(const Candidate *candidate);
unsigned long candidate_score_denominator(const Candidate *candidate);
/*
 * CandidateSet: the owned collection of Candidates one fill_suggest call returns, with the reproducibility bundle of
 * that call: random_seed, generator_version, an optional configuration (NULL when none), input_ids (the ids of what fed
 * the suggestion, count may be 0) and an optional framework_version (NULL when not available). candidates is an owned
 * array with its count (count may be 0: a run with no viable candidate still gives a valid, empty set). provenance is
 * a required ProvenanceReference naming what produced the set.
 */

typedef struct CandidateSet CandidateSet;

typedef enum {
    CANDIDATE_SET_OK = 0,
    CANDIDATE_SET_ERR_NULL_ID,
    CANDIDATE_SET_ERR_NULL_CANDIDATE,
    CANDIDATE_SET_ERR_NULL_GENERATOR_VERSION,
    CANDIDATE_SET_ERR_NULL_INPUT_ID,
    CANDIDATE_SET_ERR_NULL_PROVENANCE,
    CANDIDATE_SET_ERR_ALLOC
} CandidateSetResult;

/*
 * Takes ownership of id, each Candidate in candidates (if any),
 * configuration (if non-NULL), each EntityID in input_ids (if any), and
 * provenance on success (all destroyed by candidate_set_destroy).
 * candidates and input_ids are each optional -- pass NULL with the
 * matching count == 0 for either not supplied. generator_version is
 * copied, required (non-empty). configuration is optional -- pass NULL
 * if the generator took none. framework_version is copied when
 * non-NULL, optional -- pass NULL when not available (Framework has no
 * version yet). No entry of a non-empty candidates or input_ids array
 * may be NULL.
 */
CandidateSetResult candidate_set_create(EntityID *id, Candidate **candidates, size_t candidate_count,
                                         unsigned long random_seed, const char *generator_version,
                                         Value *configuration, EntityID **input_ids, size_t input_id_count,
                                         const char *framework_version, ProvenanceReference *provenance,
                                         CandidateSet **out);
void candidate_set_destroy(CandidateSet *set);

const Candidate *candidate_set_candidate_at(const CandidateSet *set, size_t index);
#endif
