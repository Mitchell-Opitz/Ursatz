#include <assert.h>
#include <stdio.h>

#include <stddef.h>

#include "context/context.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "rules/constraint.h"
#include "theory/framework.h"
#include "framework_test_access.h"

/*
 * Construction test: a Constraint is created as a hard, a weighted and a conditional one, refuses a missing id or framework and a WEIGHTED
 * weight that is zero-denominator or above 1, and is destroyed without destroying its framework.
 *
 * Callback test: no application evaluates a Constraint itself; the framework's callback is what is registered on it.
 * constraint_set_evaluate_fn registers, replaces and clears the callback, and is a no-op on NULL.
 */

static ConstraintEvalResult pass_callback(const Constraint *constraint, const Value *candidate, const Context *context) {
    (void)constraint;
    (void)candidate;
    (void)context;
    return CONSTRAINT_EVAL_PASS;
}

static ConstraintEvalResult fail_callback(const Constraint *constraint, const Value *candidate, const Context *context) {
    (void)constraint;
    (void)candidate;
    (void)context;
    return CONSTRAINT_EVAL_FAIL;
}

int main(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-species-counterpoint", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "species counterpoint", &framework) == FRAMEWORK_OK);

    EntityID *hard_id;
    assert(entity_id_create("constraint-no-parallel-fifths", &hard_id) == ENTITY_ID_OK);
    Constraint *hard;
    assert(constraint_create(hard_id, framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL, &hard) == CONSTRAINT_OK);

    EntityID *weighted_id;
    assert(entity_id_create("constraint-prefer-contrary-motion", &weighted_id) == ENTITY_ID_OK);
    Constraint *weighted;
    assert(constraint_create(weighted_id, framework, NULL, CONSTRAINT_TYPE_WEIGHTED, 3, 4, NULL, &weighted) ==
           CONSTRAINT_OK);

    EntityID *bad_id;
    assert(entity_id_create("constraint-bad-weight", &bad_id) == ENTITY_ID_OK);
    Constraint *bad;
    assert(constraint_create(bad_id, framework, NULL, CONSTRAINT_TYPE_WEIGHTED, 5, 4, NULL, &bad) ==
           CONSTRAINT_ERR_WEIGHT_OUT_OF_RANGE);
    entity_id_destroy(bad_id);

    EntityID *zero_id;
    assert(entity_id_create("constraint-zero-denominator", &zero_id) == ENTITY_ID_OK);
    Constraint *zero;
    assert(constraint_create(zero_id, framework, NULL, CONSTRAINT_TYPE_WEIGHTED, 0, 0, NULL, &zero) ==
           CONSTRAINT_ERR_NON_POSITIVE_DENOMINATOR);
    entity_id_destroy(zero_id);

    EntityID *missing_framework_id;
    assert(entity_id_create("constraint-no-framework", &missing_framework_id) == ENTITY_ID_OK);
    Constraint *missing;
    assert(constraint_create(NULL, framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL, &missing) ==
           CONSTRAINT_ERR_NULL_ID);
    assert(constraint_create(missing_framework_id, NULL, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL, &missing) ==
           CONSTRAINT_ERR_NULL_FRAMEWORK);
    entity_id_destroy(missing_framework_id);

    EntityID *conditional_id;
    assert(entity_id_create("constraint-conditional", &conditional_id) == ENTITY_ID_OK);
    Value *condition;
    assert(value_create("test-condition", "x", 1, &condition) == VALUE_OK);
    Constraint *conditional;
    assert(constraint_create(conditional_id, framework, NULL, CONSTRAINT_TYPE_CONDITIONAL, 0, 0, condition,
                             &conditional) == CONSTRAINT_OK);

    /* The callbacks are registered, replaced and cleared without effect on the Constraint's other fields. */
    constraint_set_evaluate_fn(NULL, pass_callback);
    constraint_set_evaluate_fn(hard, pass_callback);
    constraint_set_evaluate_fn(weighted, fail_callback);
    constraint_set_evaluate_fn(conditional, pass_callback);
    constraint_set_evaluate_fn(hard, fail_callback);
    constraint_set_evaluate_fn(hard, NULL);
    constraint_set_evaluate_fn(conditional, NULL);

    constraint_destroy(NULL);
    constraint_destroy(conditional);
    constraint_destroy(hard);
    constraint_destroy(weighted);
    framework_destroy(framework);

    printf("test_constraint: ok\n");
    return 0;
}