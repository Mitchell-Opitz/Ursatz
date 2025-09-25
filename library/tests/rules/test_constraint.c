#include <assert.h>
#include <stdio.h>

#include <stddef.h>

#include "context/context.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "rules/constraint.h"
#include "theory/framework.h"

/*
 * Construction test: a Constraint holds identity, framework/scope
 * association, and its tagged type -- including a WEIGHTED
 * constraint's weight fraction round-tripping unchanged.
 *
 * Invariant test: constraint_evaluate rejects a NULL candidate, reports
 * CONSTRAINT_EVAL_NOT_IMPLEMENTED for a Constraint with no callback, and
 * otherwise dispatches to the callback a framework registered. A CONTEXTUAL
 * constraint called without a Context is NOT_APPLICABLE.
 */

static const Constraint *g_seen_constraint;
static const Value *g_seen_candidate;
static const Context *g_seen_context;
static ConstraintEvalResult g_result;

static ConstraintEvalResult recording_callback(const Constraint *constraint, const Value *candidate,
                                               const Context *context) {
    g_seen_constraint = constraint;
    g_seen_candidate = candidate;
    g_seen_context = context;
    return g_result;
}

static ConstraintEvalResult other_callback(const Constraint *constraint, const Value *candidate,
                                           const Context *context) {
    (void)constraint;
    (void)candidate;
    (void)context;
    return CONSTRAINT_EVAL_PASS;
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

    assert(constraint_evaluate(hard, NULL, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);

    Value *candidate;
    assert(value_create("stub-candidate", "x", 1, &candidate) == VALUE_OK);
    assert(constraint_evaluate(hard, candidate, NULL) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);

    /* No callback registered: NOT_IMPLEMENTED, and the accessor says so. */
    assert(constraint_evaluate_fn(hard) == NULL);
    assert(constraint_evaluate_fn(NULL) == NULL);
    constraint_set_evaluate_fn(NULL, recording_callback);

    /* With a callback: dispatched with the same arguments, whatever it returns is passed through. */
    constraint_set_evaluate_fn(hard, recording_callback);
    assert(constraint_evaluate_fn(hard) == recording_callback);
    ConstraintEvalResult results[] = {CONSTRAINT_EVAL_PASS, CONSTRAINT_EVAL_FAIL, CONSTRAINT_EVAL_NOT_APPLICABLE,
                                      CONSTRAINT_EVAL_ERR_NULL_CANDIDATE};
    for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
        g_result = results[i];
        g_seen_constraint = NULL;
        g_seen_candidate = NULL;
        g_seen_context = (const Context *)candidate;
        assert(constraint_evaluate(hard, candidate, NULL) == results[i]);
        assert(g_seen_constraint == hard && g_seen_candidate == candidate && g_seen_context == NULL);
    }

    /* The NULL checks come before the callback. */
    g_seen_constraint = NULL;
    assert(constraint_evaluate(hard, NULL, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(constraint_evaluate(NULL, candidate, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(g_seen_constraint == NULL);

    /* A WEIGHTED constraint stays on the same contract. */
    constraint_set_evaluate_fn(weighted, other_callback);
    assert(constraint_evaluate(weighted, candidate, NULL) == CONSTRAINT_EVAL_PASS);

    /* Replacing and clearing the callback. */
    constraint_set_evaluate_fn(hard, other_callback);
    assert(constraint_evaluate(hard, candidate, NULL) == CONSTRAINT_EVAL_PASS);
    constraint_set_evaluate_fn(hard, NULL);
    assert(constraint_evaluate(hard, candidate, NULL) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);

    /* A CONTEXTUAL constraint with no Context is NOT_APPLICABLE even with a callback; with one, the callback
       decides. A CONDITIONAL constraint is not gated by the caller's Context. */
    EntityID *contextual_id;
    assert(entity_id_create("constraint-contextual", &contextual_id) == ENTITY_ID_OK);
    Constraint *contextual;
    assert(constraint_create(contextual_id, framework, NULL, CONSTRAINT_TYPE_CONTEXTUAL, 0, 0, NULL, &contextual) ==
           CONSTRAINT_OK);
    EntityID *conditional_id;
    assert(entity_id_create("constraint-conditional", &conditional_id) == ENTITY_ID_OK);
    Constraint *conditional;
    assert(constraint_create(conditional_id, framework, NULL, CONSTRAINT_TYPE_CONDITIONAL, 0, 0, NULL,
                             &conditional) == CONSTRAINT_OK);
    EntityID *context_id;
    assert(entity_id_create("context-for-constraint", &context_id) == ENTITY_ID_OK);
    Context *context;
    assert(context_create(context_id, "test", NULL, &context) == CONTEXT_OK);

    constraint_set_evaluate_fn(contextual, recording_callback);
    constraint_set_evaluate_fn(conditional, recording_callback);
    g_result = CONSTRAINT_EVAL_PASS;
    g_seen_constraint = NULL;
    assert(constraint_evaluate(contextual, candidate, NULL) == CONSTRAINT_EVAL_NOT_APPLICABLE);
    assert(g_seen_constraint == NULL);
    assert(constraint_evaluate(contextual, candidate, context) == CONSTRAINT_EVAL_PASS);
    assert(g_seen_constraint == contextual && g_seen_context == context);
    g_result = CONSTRAINT_EVAL_NOT_APPLICABLE;
    assert(constraint_evaluate(contextual, candidate, context) == CONSTRAINT_EVAL_NOT_APPLICABLE);
    g_result = CONSTRAINT_EVAL_FAIL;
    assert(constraint_evaluate(conditional, candidate, NULL) == CONSTRAINT_EVAL_FAIL);
    constraint_set_evaluate_fn(contextual, NULL);
    assert(constraint_evaluate(contextual, candidate, NULL) == CONSTRAINT_EVAL_NOT_APPLICABLE);
    assert(constraint_evaluate(contextual, candidate, context) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);

    context_destroy(context);
    constraint_destroy(contextual);
    constraint_destroy(conditional);
    value_destroy(candidate);
    constraint_destroy(hard);
    constraint_destroy(weighted);
    framework_destroy(framework);

    printf("test_constraint: ok\n");
    return 0;
}