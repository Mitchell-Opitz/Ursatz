#include <assert.h>
#include <stdio.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "rules/constraint.h"
#include "theory/framework.h"

/*
 * Construction test: a Constraint holds identity, framework/scope
 * association, and its tagged type -- including a WEIGHTED
 * constraint's weight fraction round-tripping unchanged.
 *
 * Invariant test: constraint_evaluate rejects a NULL candidate and
 * otherwise reports CONSTRAINT_EVAL_NOT_IMPLEMENTED (no evaluation
 * logic is wired up in this pass -- see constraint.h).
 */

int main(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-species-counterpoint", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "species counterpoint", &framework) == FRAMEWORK_OK);

    EntityID *hard_id;
    assert(entity_id_create("constraint-no-parallel-fifths", &hard_id) == ENTITY_ID_OK);
    Constraint *hard;
    assert(constraint_create(hard_id, framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL, &hard) == CONSTRAINT_OK);
    assert(constraint_type(hard) == CONSTRAINT_TYPE_HARD);

    EntityID *weighted_id;
    assert(entity_id_create("constraint-prefer-contrary-motion", &weighted_id) == ENTITY_ID_OK);
    Constraint *weighted;
    assert(constraint_create(weighted_id, framework, NULL, CONSTRAINT_TYPE_WEIGHTED, 3, 4, NULL, &weighted) ==
           CONSTRAINT_OK);
    assert(constraint_type(weighted) == CONSTRAINT_TYPE_WEIGHTED);
    assert(constraint_weight_numerator(weighted) == 3);
    assert(constraint_weight_denominator(weighted) == 4);

    EntityID *bad_id;
    assert(entity_id_create("constraint-bad-weight", &bad_id) == ENTITY_ID_OK);
    Constraint *bad;
    assert(constraint_create(bad_id, framework, NULL, CONSTRAINT_TYPE_WEIGHTED, 5, 4, NULL, &bad) ==
           CONSTRAINT_ERR_WEIGHT_OUT_OF_RANGE);
    entity_id_destroy(bad_id);

    assert(constraint_identity_equals(hard, hard));
    assert(!constraint_identity_equals(hard, weighted));

    assert(constraint_evaluate(hard, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);

    Value *candidate;
    assert(value_create("stub-candidate", "x", 1, &candidate) == VALUE_OK);
    assert(constraint_evaluate(hard, candidate) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);

    value_destroy(candidate);
    constraint_destroy(hard);
    constraint_destroy(weighted);
    framework_destroy(framework);

    printf("test_constraint: ok\n");
    return 0;
}