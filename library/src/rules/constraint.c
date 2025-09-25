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

const EntityID *constraint_id(const Constraint *constraint) {
    return constraint->id;
}

const Framework *constraint_framework(const Constraint *constraint) {
    return constraint->framework;
}

const Value *constraint_scope(const Constraint *constraint) {
    return constraint->scope;
}

ConstraintType constraint_type(const Constraint *constraint) {
    return constraint->type;
}

unsigned long constraint_weight_numerator(const Constraint *constraint) {
    return constraint->weight_numerator;
}

unsigned long constraint_weight_denominator(const Constraint *constraint) {
    return constraint->weight_denominator;
}

const Value *constraint_condition(const Constraint *constraint) {
    return constraint->condition;
}

bool constraint_identity_equals(const Constraint *a, const Constraint *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}

void constraint_set_evaluate_fn(Constraint *constraint, ConstraintEvaluateFn evaluate) {
    if (constraint == NULL) {
        return;
    }
    constraint->evaluate = evaluate;
}

ConstraintEvaluateFn constraint_evaluate_fn(const Constraint *constraint) {
    return constraint == NULL ? NULL : constraint->evaluate;
}

ConstraintEvalResult constraint_evaluate(const Constraint *constraint, const Value *candidate, const Context *context) {
    if (constraint == NULL || candidate == NULL) {
        return CONSTRAINT_EVAL_ERR_NULL_CANDIDATE;
    }
    /* A CONTEXTUAL constraint's condition is about ambient context; with none supplied it cannot hold. */
    if (constraint->type == CONSTRAINT_TYPE_CONTEXTUAL && context == NULL) {
        return CONSTRAINT_EVAL_NOT_APPLICABLE;
    }
    if (constraint->evaluate == NULL) {
        return CONSTRAINT_EVAL_NOT_IMPLEMENTED;
    }
    return constraint->evaluate(constraint, candidate, context);
}
