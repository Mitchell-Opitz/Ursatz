#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "rules/rule_evaluation.h"

/*
 * Construction test: a RuleEvaluation holds a rule_id reference, a
 * tagged outcome (not a boolean), an optional owned finding
 * (Interpretation), and an optional weight fraction -- round-tripping
 * unchanged.
 *
 * Invariant test: rule_evaluation_create rejects a NULL rule_id and a
 * weight fraction with numerator > denominator (when a weight is
 * actually recorded). A weight_denominator of 0 is the "no weight
 * recorded" sentinel and is always accepted regardless of numerator.
 */

static Interpretation *make_finding(void) {
    EntityID *id;
    assert(entity_id_create("interpretation-parallel-fifths", &id) == ENTITY_ID_OK);
    EntityID *target_id;
    assert(entity_id_create("candidate-1", &target_id) == ENTITY_ID_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    Uncertainty *uncertainty;
    assert(uncertainty_create_scalar_confidence(1, 1, &uncertainty) == UNCERTAINTY_OK);
    Value *claim;
    const char *content = "parallel fifths between voices 1 and 2";
    assert(value_create("Claim", content, strlen(content) + 1, &claim) == VALUE_OK);

    Interpretation *finding;
    assert(interpretation_create(id, target_id, claim, NULL, NULL, NULL, uncertainty, "rule-evaluator",
                                  "constraint-evaluation", INTERPRETATION_STATUS_MACHINE_GENERATED, provenance,
                                  &finding) == INTERPRETATION_OK);
    return finding;
}

int main(void) {
    EntityID *rule_id;
    assert(entity_id_create("rule-no-parallel-fifths", &rule_id) == ENTITY_ID_OK);
    Interpretation *finding = make_finding();

    RuleEvaluation *evaluation;
    assert(rule_evaluation_create(rule_id, RULE_EVAL_VIOLATED, finding, 3, 4, &evaluation) == RULE_EVALUATION_OK);

    EntityID *expected_rule_id;
    assert(entity_id_create("rule-no-parallel-fifths", &expected_rule_id) == ENTITY_ID_OK);
    assert(entity_id_equals(rule_evaluation_rule_id(evaluation), expected_rule_id));
    entity_id_destroy(expected_rule_id);

    assert(rule_evaluation_outcome(evaluation) == RULE_EVAL_VIOLATED);
    assert(rule_evaluation_finding(evaluation) == finding);
    assert(rule_evaluation_weight_numerator(evaluation) == 3);
    assert(rule_evaluation_weight_denominator(evaluation) == 4);

    rule_evaluation_destroy(evaluation);

    /* No finding, no weight recorded -- both optional. */
    EntityID *bare_rule_id;
    assert(entity_id_create("rule-prefer-contrary-motion", &bare_rule_id) == ENTITY_ID_OK);
    RuleEvaluation *bare;
    assert(rule_evaluation_create(bare_rule_id, RULE_EVAL_NOT_APPLICABLE, NULL, 0, 0, &bare) == RULE_EVALUATION_OK);
    assert(rule_evaluation_outcome(bare) == RULE_EVAL_NOT_APPLICABLE);
    assert(rule_evaluation_finding(bare) == NULL);
    assert(rule_evaluation_weight_denominator(bare) == 0);
    rule_evaluation_destroy(bare);

    /* NULL rule_id is rejected. */
    RuleEvaluation *rejected;
    assert(rule_evaluation_create(NULL, RULE_EVAL_NOT_IMPLEMENTED, NULL, 0, 0, &rejected) ==
           RULE_EVALUATION_ERR_NULL_RULE_ID);

    /* An out-of-range weight (when a weight is actually recorded) is rejected. */
    EntityID *bad_weight_id;
    assert(entity_id_create("rule-bad-weight", &bad_weight_id) == ENTITY_ID_OK);
    RuleEvaluation *bad_weight;
    assert(rule_evaluation_create(bad_weight_id, RULE_EVAL_SATISFIED, NULL, 5, 4, &bad_weight) ==
           RULE_EVALUATION_ERR_WEIGHT_OUT_OF_RANGE);
    entity_id_destroy(bad_weight_id);

    printf("test_rule_evaluation: ok\n");
    return 0;
}