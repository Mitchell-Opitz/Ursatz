#ifndef MUSIC_INTERPRETATION_HYPOTHESIS_H
#define MUSIC_INTERPRETATION_HYPOTHESIS_H

#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Hypothesis (Category / base type): a candidate explanation (TDD
 * §83).
 *
 * Fields: observation refs, evidence refs, candidate set.
 *
 * A Hypothesis MUST support multiple candidates -- it is NOT modeled
 * as a single value with a confidence score only. candidates is an
 * array (candidate_create takes candidate_count >= 1, and nothing in
 * this API caps it at one), each with its own label and plausibility
 * fraction, so a Hypothesis can represent several competing
 * explanations at once (e.g. "this sonority is a V7" vs "this sonority
 * is a passing tone" as two candidates of the same Hypothesis).
 *
 * observation_ids/evidence_ids are opaque EntityID references
 * (identity/entity_id.h), not embedded Observation/Evidence objects --
 * the same reference-only pattern used throughout this codebase.
 * observation_ids is required (a Hypothesis explains at least one
 * Observation); evidence_ids may be empty (evidence not yet gathered
 * is a valid, if weak, Hypothesis state).
 *
 * Multiple Hypotheses may reference the same Observation without
 * conflict: this type holds only EntityID references, never a claim of
 * exclusive ownership over an Observation.
 */

typedef struct {
    char *label;
    unsigned long plausibility_numerator;
    unsigned long plausibility_denominator;
} HypothesisCandidate;

typedef struct Hypothesis Hypothesis;

typedef enum {
    HYPOTHESIS_OK = 0,
    HYPOTHESIS_ERR_NULL_ID,
    HYPOTHESIS_ERR_NULL_OBSERVATION_IDS,
    HYPOTHESIS_ERR_NULL_OBSERVATION_ID,
    HYPOTHESIS_ERR_NULL_EVIDENCE_ID,
    HYPOTHESIS_ERR_NULL_CANDIDATES,
    HYPOTHESIS_ERR_NULL_CANDIDATE_LABEL,
    HYPOTHESIS_ERR_ALLOC
} HypothesisResult;

/*
 * Takes ownership of id and of observation_ids/evidence_ids (and each
 * EntityID they point to) on success -- all destroyed by
 * hypothesis_destroy. candidates is copied (including each label);
 * caller retains ownership of the array/labels passed in.
 *
 * observation_ids must be non-NULL with observation_id_count > 0; no
 * entry may be NULL. evidence_ids may be NULL with
 * evidence_id_count == 0; if evidence_id_count > 0, evidence_ids must
 * be non-NULL with no NULL entries. candidates must be non-NULL with
 * candidate_count > 0; no entry's label may be NULL.
 */
HypothesisResult hypothesis_create(EntityID *id, EntityID **observation_ids, size_t observation_id_count,
                                    EntityID **evidence_ids, size_t evidence_id_count,
                                    const HypothesisCandidate *candidates, size_t candidate_count,
                                    Hypothesis **out);
void hypothesis_destroy(Hypothesis *hypothesis);

const EntityID *hypothesis_id(const Hypothesis *hypothesis);

size_t hypothesis_observation_id_count(const Hypothesis *hypothesis);
const EntityID *hypothesis_observation_id_at(const Hypothesis *hypothesis, size_t index);

size_t hypothesis_evidence_id_count(const Hypothesis *hypothesis);
const EntityID *hypothesis_evidence_id_at(const Hypothesis *hypothesis, size_t index);

size_t hypothesis_candidate_count(const Hypothesis *hypothesis);
const char *hypothesis_candidate_label_at(const Hypothesis *hypothesis, size_t index);
unsigned long hypothesis_candidate_plausibility_numerator_at(const Hypothesis *hypothesis, size_t index);
unsigned long hypothesis_candidate_plausibility_denominator_at(const Hypothesis *hypothesis, size_t index);

#endif