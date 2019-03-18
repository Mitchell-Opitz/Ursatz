#ifndef MUSIC_THEORY_TENDENCY_H
#define MUSIC_THEORY_TENDENCY_H

#include <stdbool.h>

#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Tendency (Category / base type): observed statistical behavior (TDD
 * §99).
 *
 * A Tendency's frequency is an exact rational fraction (numerator/
 * denominator, Law 19, never float/double) describing how often the
 * observed behavior occurs -- and nothing more. It MUST NEVER
 * automatically imply must/must-not/valid/invalid: this header defines
 * no PASS/FAIL/OK-as-validity vocabulary anywhere, deliberately, and
 * exposes no evaluate()-shaped API the way Constraint does
 * (rules/constraint.h). A Tendency cannot be constructed from, or
 * coerced into, a Rule/Constraint result type -- TendencyResult is a
 * disjoint enum with no shared values or shared meaning with RuleResult
 * or ConstraintResult/ConstraintEvalResult.
 *
 * observed_data is kept as an opaque, nullable Value (core/value.h):
 * per the OPEN ISSUE (Dependency Spec §7.2, Theory -> Corpus), this
 * type accepts corpus-derived statistics only as an already-computed
 * input value, never as a music-corpus module dependency. There is no
 * include of any music-corpus header anywhere in this module.
 */

typedef struct Tendency Tendency;

typedef enum {
    TENDENCY_OK = 0,
    TENDENCY_ERR_NULL_ID,
    TENDENCY_ERR_NULL_LABEL,
    TENDENCY_ERR_NON_POSITIVE_DENOMINATOR,
    TENDENCY_ERR_FREQUENCY_OUT_OF_RANGE,
    TENDENCY_ERR_ALLOC
} TendencyResult;

/*
 * Takes ownership of id (and of observed_data, if non-NULL) on success
 * -- both destroyed by tendency_destroy. label is copied, required,
 * non-empty. frequency is a fraction in [0, 1]: numerator <=
 * denominator, denominator > 0. observed_data is optional -- pass NULL
 * if not supplied.
 */
TendencyResult tendency_create(EntityID *id, const char *label, unsigned long frequency_numerator,
                                unsigned long frequency_denominator, Value *observed_data, Tendency **out);
void tendency_destroy(Tendency *tendency);

const EntityID *tendency_id(const Tendency *tendency);
const char *tendency_label(const Tendency *tendency);
unsigned long tendency_frequency_numerator(const Tendency *tendency);
unsigned long tendency_frequency_denominator(const Tendency *tendency);
const Value *tendency_observed_data(const Tendency *tendency);

bool tendency_identity_equals(const Tendency *a, const Tendency *b);

#endif