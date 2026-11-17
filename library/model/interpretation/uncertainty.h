#ifndef MUSIC_INTERPRETATION_UNCERTAINTY_H
#define MUSIC_INTERPRETATION_UNCERTAINTY_H

/*
 * Uncertainty (Concept/Value, TDD §84, explicitly first-class):
 * uncertainty MUST support more than a single scalar confidence value
 * -- unknown, interval estimate, probability distribution, confidence
 * score, candidate set, credible interval, qualitative uncertainty,
 * and competing hypotheses are all named as distinct shapes (TDD
 * §84-86).
 *
 * This pass builds one shape, the scalar confidence the codebase uses
 * everywhere. The others (unknown, candidate set, interval estimate,
 * probability distribution, credible interval, qualitative uncertainty)
 * are NOT built here -- a new shape gets its own UncertaintyKind variant
 * and constructor rather than being forced into the scalar one. The
 * UNKNOWN and CANDIDATE_SET kinds are named for that later work.
 *
 * Confidence is represented as an exact rational fraction
 * (numerator/denominator), never float/double (Law 19), consistent with
 * Duration and MusicalTime elsewhere in this codebase.
 *
 * Querying a field that does not belong to the current kind is a
 * programming error (the accessor's precondition), not a silent
 * zero/default.
 */

typedef enum {
    UNCERTAINTY_KIND_UNKNOWN,
    UNCERTAINTY_KIND_SCALAR_CONFIDENCE,
    UNCERTAINTY_KIND_CANDIDATE_SET
} UncertaintyKind;

typedef struct Uncertainty Uncertainty;

typedef enum {
    UNCERTAINTY_OK = 0,
    UNCERTAINTY_ERR_NON_POSITIVE_DENOMINATOR,
    UNCERTAINTY_ERR_CONFIDENCE_OUT_OF_RANGE,
    UNCERTAINTY_ERR_ALLOC
} UncertaintyResult;

/* confidence_numerator/confidence_denominator MUST express a value in
   [0, 1] (numerator <= denominator), denominator > 0. */
UncertaintyResult uncertainty_create_scalar_confidence(unsigned long confidence_numerator,
                                                        unsigned long confidence_denominator, Uncertainty **out);

void uncertainty_destroy(Uncertainty *uncertainty);

UncertaintyKind uncertainty_kind(const Uncertainty *uncertainty);

/* Valid only when uncertainty_kind == UNCERTAINTY_KIND_SCALAR_CONFIDENCE. */
unsigned long uncertainty_scalar_confidence_numerator(const Uncertainty *uncertainty);
unsigned long uncertainty_scalar_confidence_denominator(const Uncertainty *uncertainty);

#endif