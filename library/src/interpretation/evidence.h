#ifndef MUSIC_INTERPRETATION_EVIDENCE_H
#define MUSIC_INTERPRETATION_EVIDENCE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Evidence (Category / base type): the structured basis for an
 * assertion (TDD §103).
 *
 * Fields per TDD §104: strength, relevance, reliability, independence,
 * method, source. These MUST NOT collapse to a single scalar "evidence
 * score" -- each is its own field, independently settable and
 * independently queryable. strength/relevance/reliability are each
 * represented as exact rational fractions in [0, 1]
 * (numerator/denominator), never float/double (Law 19), the same
 * representation Uncertainty (interpretation/uncertainty.h) uses for
 * its confidence values.
 *
 * Evidence is distinguishable from the Interpretation/Hypothesis it
 * supports: it is its own object with its own identity (EntityID),
 * referenced by id from those types (see Relationship's evidence_id,
 * relationship/relationship.h), never embedded/duplicated into them.
 */

typedef struct Evidence Evidence;

typedef enum {
    EVIDENCE_OK = 0,
    EVIDENCE_ERR_NULL_ID,
    EVIDENCE_ERR_NON_POSITIVE_DENOMINATOR,
    EVIDENCE_ERR_SCORE_OUT_OF_RANGE,
    EVIDENCE_ERR_NULL_METHOD,
    EVIDENCE_ERR_NULL_SOURCE,
    EVIDENCE_ERR_ALLOC
} EvidenceResult;

/*
 * Takes ownership of id on success (destroyed by evidence_destroy).
 * method and source are required (copied, non-empty).
 *
 * Each of strength/relevance/reliability is a fraction in [0, 1]:
 * numerator <= denominator, denominator > 0.
 */
EvidenceResult evidence_create(EntityID *id, unsigned long strength_numerator, unsigned long strength_denominator,
                                unsigned long relevance_numerator, unsigned long relevance_denominator,
                                unsigned long reliability_numerator, unsigned long reliability_denominator,
                                bool independence, const char *method, const char *source, Evidence **out);
void evidence_destroy(Evidence *evidence);

const EntityID *evidence_id(const Evidence *evidence);

unsigned long evidence_strength_numerator(const Evidence *evidence);
unsigned long evidence_strength_denominator(const Evidence *evidence);

unsigned long evidence_relevance_numerator(const Evidence *evidence);
unsigned long evidence_relevance_denominator(const Evidence *evidence);

unsigned long evidence_reliability_numerator(const Evidence *evidence);
unsigned long evidence_reliability_denominator(const Evidence *evidence);

bool evidence_independence(const Evidence *evidence);
const char *evidence_method(const Evidence *evidence);
const char *evidence_source(const Evidence *evidence);

#endif