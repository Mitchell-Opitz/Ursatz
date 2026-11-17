#ifndef MUSIC_RULES_CONSTRAINT_H
#define MUSIC_RULES_CONSTRAINT_H

#include <stdbool.h>

#include "theory/rules/context.h"
#include "model/core/value.h"
#include "model/identity/entity_id.h"
#include "theory/framework/framework.h"

/*
 * Constraint (Category / base type): restricts candidate states (TDD
 * §96).
 *
 * A Constraint has an id, a framework, a scope and a tagged type
 * (CONSTRAINT_TYPE_HARD/_SOFT/_WEIGHTED/_CONDITIONAL/_CONTEXTUAL), and a
 * WEIGHTED one a weight fraction (exact rational per Law 19, never
 * float/double). framework is a reference only, not owned. scope and the
 * condition of a CONDITIONAL or CONTEXTUAL constraint are opaque, nullable
 * Values (core/value.h), minimal placeholders rather than frozen schemas.
 *
 * What a caller reads from a Constraint is its callback: the evaluation
 * callback (ConstraintEvaluateFn) the Constraint's framework registers on it
 * with constraint_set_evaluate_fn. The Constraint holds only that pointer:
 * the framework owns the callback and whatever data is behind it, and nothing
 * owned is embedded in Constraint. The callback returns one
 * ConstraintEvalResult for every ConstraintType: CONSTRAINT_EVAL_PASS,
 * _FAIL, _NOT_APPLICABLE (a CONDITIONAL or CONTEXTUAL constraint whose
 * condition does not hold, kept distinct from _FAIL so a caller aggregating
 * scores never conflates "did not apply" with "failed"), or
 * _ERR_NULL_CANDIDATE. CONSTRAINT_EVAL_NOT_IMPLEMENTED is the result for a
 * Constraint with no callback.
 */

typedef struct Constraint Constraint;

typedef enum {
    CONSTRAINT_TYPE_HARD = 0,
    CONSTRAINT_TYPE_SOFT,
    CONSTRAINT_TYPE_WEIGHTED,
    CONSTRAINT_TYPE_CONDITIONAL,
    CONSTRAINT_TYPE_CONTEXTUAL
} ConstraintType;

typedef enum {
    CONSTRAINT_OK = 0,
    CONSTRAINT_ERR_NULL_ID,
    CONSTRAINT_ERR_NULL_FRAMEWORK,
    CONSTRAINT_ERR_NON_POSITIVE_DENOMINATOR,
    CONSTRAINT_ERR_WEIGHT_OUT_OF_RANGE,
    CONSTRAINT_ERR_ALLOC
} ConstraintResult;

typedef enum {
    CONSTRAINT_EVAL_PASS = 0,
    CONSTRAINT_EVAL_FAIL,
    CONSTRAINT_EVAL_NOT_APPLICABLE,
    CONSTRAINT_EVAL_ERR_NULL_CANDIDATE,
    CONSTRAINT_EVAL_NOT_IMPLEMENTED
} ConstraintEvalResult;

/*
 * Evaluation callback a framework registers on a Constraint (see
 * constraint_set_evaluate_fn). candidate is non-NULL; context may be NULL.
 * Both are referenced only for the duration of the call.
 */
typedef ConstraintEvalResult (*ConstraintEvaluateFn)(const Constraint *constraint, const Value *candidate,
                                                      const Context *context);

/*
 * Takes ownership of id (and of scope/condition, if non-NULL) on
 * success -- all destroyed by constraint_destroy. framework is
 * required and referenced only, not owned; it must outlive this
 * Constraint. scope and condition are optional -- pass NULL if not
 * supplied. weight_numerator/weight_denominator are only meaningful
 * when type is CONSTRAINT_TYPE_WEIGHTED (denominator > 0, numerator <=
 * denominator); ignored for every other type.
 */
ConstraintResult constraint_create(EntityID *id, const Framework *framework, Value *scope, ConstraintType type,
                                    unsigned long weight_numerator, unsigned long weight_denominator,
                                    Value *condition, Constraint **out);
void constraint_destroy(Constraint *constraint);

/*
 * Registers (or, with NULL, clears) the evaluation callback. Constraint
 * keeps the pointer only and never owns what is behind it. A Constraint
 * starts with no callback. No-op on a NULL constraint.
 */
void constraint_set_evaluate_fn(Constraint *constraint, ConstraintEvaluateFn evaluate);

#endif