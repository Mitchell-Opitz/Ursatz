#include "rules/constraint.h"

#include <stdlib.h>

struct Constraint {
    EntityID *id;
    const Framework *framework;
    Value *scope;
    ConstraintType type;
    unsigned long weight_numerator;
    unsigned long weight_denominator;
    Value *condition;
    ConstraintEvaluateFn evaluate;
};

ConstraintResult constraint_create(EntityID *id, const Framework *framework, Value *scope, ConstraintType type,
                                    unsigned long weight_numerator, unsigned long weight_denominator,
                                    Value *condition, Constraint **out) {
    if (id == NULL) {
        return CONSTRAINT_ERR_NULL_ID;
    }
    if (framework == NULL) {
        return CONSTRAINT_ERR_NULL_FRAMEWORK;
    }
    if (type == CONSTRAINT_TYPE_WEIGHTED) {
        if (weight_denominator == 0) {
            return CONSTRAINT_ERR_NON_POSITIVE_DENOMINATOR;
        }
        if (weight_numerator > weight_denominator) {
            return CONSTRAINT_ERR_WEIGHT_OUT_OF_RANGE;
        }
    }

    Constraint *constraint = (Constraint *)malloc(sizeof(Constraint));
    if (constraint == NULL) {
        return CONSTRAINT_ERR_ALLOC;
    }

    constraint->id = id;
    constraint->framework = framework;
    constraint->scope = scope;
    constraint->type = type;
    constraint->weight_numerator = weight_numerator;
    constraint->weight_denominator = weight_denominator;
    constraint->condition = condition;
    constraint->evaluate = NULL;

    *out = constraint;
    return CONSTRAINT_OK;
}

void constraint_destroy(Constraint *constraint) {
    if (constraint == NULL) {
        return;
    }
    entity_id_destroy(constraint->id);
    value_destroy(constraint->scope);
    value_destroy(constraint->condition);
    free(constraint);
}

void constraint_set_evaluate_fn(Constraint *constraint, ConstraintEvaluateFn evaluate) {
    if (constraint == NULL) {
        return;
    }
    constraint->evaluate = evaluate;
}
