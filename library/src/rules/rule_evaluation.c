#include "rules/rule_evaluation.h"

#include <stdlib.h>

struct RuleEvaluation {
    EntityID *rule_id;
    RuleEvalOutcome outcome;
    Interpretation *finding;
    unsigned long weight_numerator;
    unsigned long weight_denominator;
};

RuleEvaluationResult rule_evaluation_create(EntityID *rule_id, RuleEvalOutcome outcome, Interpretation *finding,
                                             unsigned long weight_numerator, unsigned long weight_denominator,
                                             RuleEvaluation **out) {
    if (rule_id == NULL) {
        return RULE_EVALUATION_ERR_NULL_RULE_ID;
    }
    if (weight_denominator > 0 && weight_numerator > weight_denominator) {
        return RULE_EVALUATION_ERR_WEIGHT_OUT_OF_RANGE;
    }

    RuleEvaluation *evaluation = (RuleEvaluation *)malloc(sizeof(RuleEvaluation));
    if (evaluation == NULL) {
        return RULE_EVALUATION_ERR_ALLOC;
    }

    evaluation->rule_id = rule_id;
    evaluation->outcome = outcome;
    evaluation->finding = finding;
    evaluation->weight_numerator = weight_numerator;
    evaluation->weight_denominator = weight_denominator;

    *out = evaluation;
    return RULE_EVALUATION_OK;
}

void rule_evaluation_destroy(RuleEvaluation *evaluation) {
    if (evaluation == NULL) {
        return;
    }
    entity_id_destroy(evaluation->rule_id);
    interpretation_destroy(evaluation->finding);
    free(evaluation);
}

const EntityID *rule_evaluation_rule_id(const RuleEvaluation *evaluation) {
    return evaluation->rule_id;
}

RuleEvalOutcome rule_evaluation_outcome(const RuleEvaluation *evaluation) {
    return evaluation->outcome;
}

const Interpretation *rule_evaluation_finding(const RuleEvaluation *evaluation) {
    return evaluation->finding;
}

unsigned long rule_evaluation_weight_numerator(const RuleEvaluation *evaluation) {
    return evaluation->weight_numerator;
}

unsigned long rule_evaluation_weight_denominator(const RuleEvaluation *evaluation) {
    return evaluation->weight_denominator;
}