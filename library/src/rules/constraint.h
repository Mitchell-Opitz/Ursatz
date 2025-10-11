#ifndef MUSIC_RULES_CONSTRAINT_H
#define MUSIC_RULES_CONSTRAINT_H

#include <stdbool.h>

#include "context/context.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/framework.h"

/*
 * Constraint (Category / base type): restricts candidate states (TDD
 * §96).
 *
 * Fields: id, framework, scope, type, and type-specific data.
 *
 * framework is a reference only, not owned -- same pattern as Rule
 * (rules/rule.h) and Context's parent (context/context.h).
 *
 * type is a tagged type (CONSTRAINT_TYPE_HARD/_SOFT/_WEIGHTED/
 * _CONDITIONAL/_CONTEXTUAL, TDD §96), not a single boolean "is_hard"
 * flag: a Constraint with type WEIGHTED carries a weight fraction
 * (numerator/denominator, exact rational per Law 19, never
 * float/double -- the same representation Evidence uses,
 * interpretation/evidence.h); a Constraint with type CONDITIONAL or
 * CONTEXTUAL carries an opaque, nullable condition Value
 * (core/value.h), a minimal placeholder rather than a frozen
 * conditional-expression schema. weight and condition are unused
 * (ignored) for types that do not need them.
 *
 * scope is kept as an opaque, nullable Value, same placeholder
 * treatment Rule and Relationship give their own scope fields.
 *
 * constraint_evaluate (TDD §96) decides whether a candidate satisfies a
 * Constraint by calling the evaluation callback (ConstraintEvaluateFn) the
 * Constraint's framework registered on it. Constraint holds only that
 * pointer: the framework owns the callback and whatever data is behind it,
 * and nothing owned is embedded in Constraint. A Constraint with no callback
 * reports CONSTRAINT_EVAL_NOT_IMPLEMENTED. Frameworks that never register one
 * are unaffected.
 *
 * The result contract is the same for every ConstraintType:
 * CONSTRAINT_EVAL_PASS, _FAIL, _NOT_APPLICABLE, or _ERR_NULL_CANDIDATE.
 * A WEIGHTED constraint stays on this contract too: its weight is read
 * separately through constraint_weight_numerator/_denominator.
 *
 * constraint_evaluate takes an optional context (context/context.h):
 * CONDITIONAL and CONTEXTUAL constraints need ambient context to decide
 * whether their condition even applies. context is a reference only, not
 * owned. CONSTRAINT_EVAL_NOT_APPLICABLE is the result for a CONDITIONAL/
 * CONTEXTUAL constraint whose condition does not hold for the given
 * candidate/context -- kept distinct from CONSTRAINT_EVAL_FAIL so a caller
 * aggregating scores downstream never conflates "did not apply" with
 * "failed." constraint_evaluate itself returns it only for a CONTEXTUAL
 * constraint called with no context (the condition is about ambient context,
 * so with none it cannot hold); the condition Value is opaque, so any other
 * condition is the callback's to decide.
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
 * constraint_set_evaluate_fn). A Constraint holds only this pointer: the
 * framework owns the callback and any data behind it, and nothing owned is
 * embedded in Constraint. candidate and context are the arguments given to
 * constraint_evaluate (already checked non-NULL for candidate; context may be
 * NULL) and are referenced only for the duration of the call.
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

const EntityID *constraint_id(const Constraint *constraint);
const Framework *constraint_framework(const Constraint *constraint);
const Value *constraint_scope(const Constraint *constraint);
ConstraintType constraint_type(const Constraint *constraint);
unsigned long constraint_weight_numerator(const Constraint *constraint);
unsigned long constraint_weight_denominator(const Constraint *constraint);
const Value *constraint_condition(const Constraint *constraint);

bool constraint_identity_equals(const Constraint *a, const Constraint *b);

/*
 * Registers (or, with NULL, clears) the evaluation callback. Constraint
 * keeps the pointer only and never owns what is behind it. A Constraint
 * starts with no callback. No-op on a NULL constraint.
 */
void constraint_set_evaluate_fn(Constraint *constraint, ConstraintEvaluateFn evaluate);

/* The registered callback, or NULL if none (or constraint is NULL). */
ConstraintEvaluateFn constraint_evaluate_fn(const Constraint *constraint);

/*
 * Evaluates candidate against constraint -- see file header. Returns
 * ERR_NULL_CANDIDATE for a NULL constraint or candidate, NOT_APPLICABLE for a
 * CONTEXTUAL constraint with a NULL context, otherwise the registered
 * callback's result, or NOT_IMPLEMENTED when none is registered. context is
 * optional -- pass NULL if not supplied.
 */
ConstraintEvalResult constraint_evaluate(const Constraint *constraint, const Value *candidate, const Context *context);

#endif