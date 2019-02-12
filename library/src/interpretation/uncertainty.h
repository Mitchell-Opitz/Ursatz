#ifndef MUSIC_INTERPRETATION_UNCERTAINTY_H
#define MUSIC_INTERPRETATION_UNCERTAINTY_H

#include <stddef.h>

/*
 * Uncertainty (Concept/Value, TDD §84, explicitly first-class):
 * uncertainty MUST support more than a single scalar confidence value
 * -- unknown, interval estimate, probability distribution, confidence
 * score, candidate set, credible interval, qualitative uncertainty,
 * and competing hypotheses are all named as distinct shapes (TDD
 * §84-86).
 *
 * This pass implements a small tagged/variant type covering the three
 * shapes the codebase needs right now: unknown, scalar confidence, and
 * candidate set. The remaining shapes (interval estimate, probability
 * distribution, credible interval, qualitative uncertainty) are NOT
 * built here -- add new UncertaintyKind variants for them later rather
 * than forcing them into one of these three.
 *
 * Confidence and candidate weights are represented as exact rational
 * fractions (numerator/denominator), never float/double (Law 19),
 * consistent with Duration and MusicalTime elsewhere in this codebase.
 *
 * "unknown" and "known approximately" (scalar confidence) and "known
 * candidates with probabilities" (candidate set) are distinct kinds --
 * this type never collapses them into one shape (TDD §86): querying a
 * field that does not belong to the current kind is a programming
 * error (the accessor's precondition), not a silent zero/default.
 */

typedef enum {
    UNCERTAINTY_KIND_UNKNOWN,
    UNCERTAINTY_KIND_SCALAR_CONFIDENCE,
    UNCERTAINTY_KIND_CANDIDATE_SET
} UncertaintyKind;

typedef struct {
    char *label;
    unsigned long weight_numerator;
    unsigned long weight_denominator;
} UncertaintyCandidate;

typedef struct Uncertainty Uncertainty;

typedef enum {
    UNCERTAINTY_OK = 0,
    UNCERTAINTY_ERR_NON_POSITIVE_DENOMINATOR,
    UNCERTAINTY_ERR_CONFIDENCE_OUT_OF_RANGE,
    UNCERTAINTY_ERR_NULL_CANDIDATES,
    UNCERTAINTY_ERR_NULL_CANDIDATE_LABEL,
    UNCERTAINTY_ERR_ALLOC
} UncertaintyResult;

UncertaintyResult uncertainty_create_unknown(Uncertainty **out);

/* confidence_numerator/confidence_denominator MUST express a value in
   [0, 1] (numerator <= denominator), denominator > 0. */
UncertaintyResult uncertainty_create_scalar_confidence(unsigned long confidence_numerator,
                                                        unsigned long confidence_denominator, Uncertainty **out);

/*
 * Takes ownership of nothing (candidates and each label are copied).
 * candidates must be non-NULL with candidate_count > 0; no entry's
 * label may be NULL.
 */
UncertaintyResult uncertainty_create_candidate_set(const UncertaintyCandidate *candidates,
                                                    size_t candidate_count, Uncertainty **out);

void uncertainty_destroy(Uncertainty *uncertainty);

UncertaintyKind uncertainty_kind(const Uncertainty *uncertainty);

/* Valid only when uncertainty_kind == UNCERTAINTY_KIND_SCALAR_CONFIDENCE. */
unsigned long uncertainty_scalar_confidence_numerator(const Uncertainty *uncertainty);
unsigned long uncertainty_scalar_confidence_denominator(const Uncertainty *uncertainty);

/* Valid only when uncertainty_kind == UNCERTAINTY_KIND_CANDIDATE_SET. */
size_t uncertainty_candidate_count(const Uncertainty *uncertainty);
const char *uncertainty_candidate_label_at(const Uncertainty *uncertainty, size_t index);
unsigned long uncertainty_candidate_weight_numerator_at(const Uncertainty *uncertainty, size_t index);
unsigned long uncertainty_candidate_weight_denominator_at(const Uncertainty *uncertainty, size_t index);

#endif