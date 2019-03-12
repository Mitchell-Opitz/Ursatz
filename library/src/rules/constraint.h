#ifndef MUSIC_RULES_CONSTRAINT_H
#define MUSIC_RULES_CONSTRAINT_H

#include <stdbool.h>

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
 * constraint_evaluate is a machine-evaluable stub (TDD §96): it only
 * validates its own arguments and reports that no evaluation logic is
 * wired up yet. Actually deciding whether a candidate satisfies a
 * Constraint is a generation-pipeline concern (music-generation, L8)
 * and is out of scope for this pass -- this stub exists so an L8
 * consumer has a stable call shape to wire up against, not to perform
 * evaluation itself.
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
    CONSTRAINT_EVAL_ERR_NULL_CANDIDATE,
    CONSTRAINT_EVAL_NOT_IMPLEMENTED
} ConstraintEvalResult;

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

/* Stub -- see file header. Never performs evaluation logic in this pass. */
ConstraintEvalResult constraint_evaluate(const Constraint *constraint, const Value *candidate);

#endif