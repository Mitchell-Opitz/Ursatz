#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/candidate.h"
#include "generation/candidate_evaluator.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "theory/framework.h"
#include "theory/statistical_model.h"

/*
 * CandidateEvaluation: construction, accessors, explainability -- score
 * is always accompanied by evaluations/violations, never a bare number.
 *
 * Interface-contract test: a concrete CandidateEvaluator is a name plus
 * a CandidateEvaluatorFn, invoked as evaluate(candidate, rules,
 * constraints, preferences, statistical_models) -> out (TDD Sec159).
 * This mock evaluator stands in for a real one (out of scope for this
 * pass) purely to exercise the interface's shape: candidate and the L7
 * arrays are borrowed (never destroyed by the evaluator), a NULL
 * candidate is rejected without touching *out, and a successful call
 * hands back a CandidateEvaluation the caller owns.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Interpretation *make_interpretation(const char *id_value, const char *claim_content) {
    Value *claim;
    assert(value_create("EvaluationClaim", claim_content, strlen(claim_content) + 1, &claim) == VALUE_OK);
    Uncertainty *uncertainty;
    assert(uncertainty_create_unknown(&uncertainty) == UNCERTAINTY_OK);
    Interpretation *interpretation;
    assert(interpretation_create(make_id(id_value), make_id("target-mock"), claim, NULL, NULL, uncertainty,
                                  "mock-evaluator", "mock-method", INTERPRETATION_STATUS_MACHINE_GENERATED,
                                  make_absent_provenance(), &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

static CandidateEvaluatorResult mock_evaluate(const Candidate *candidate, const Rule **rules, size_t rule_count,
                                               const Constraint **constraints, size_t constraint_count,
                                               const Preference **preferences, size_t preference_count,
                                               const StatisticalModel **statistical_models,
                                               size_t statistical_model_count, CandidateEvaluation **out) {
    (void)rules;
    (void)rule_count;
    (void)constraints;
    (void)constraint_count;
    (void)preferences;
    (void)preference_count;
    (void)statistical_models;
    (void)statistical_model_count;

    if (candidate == NULL) {
        return CANDIDATE_EVALUATOR_ERR_NULL_CANDIDATE;
    }
    if (out == NULL) {
        return CANDIDATE_EVALUATOR_ERR_NULL_OUT;
    }

    Interpretation *evaluations[1] = {make_interpretation("evaluation-mock-1", "stays within vocal range")};
    Interpretation *violations[1] = {make_interpretation("violation-mock-1", "parallel fifths at m.3")};

    CandidateEvaluation *evaluation;
    if (candidate_evaluation_create(evaluations, 1, violations, 1, 3, 4, make_absent_provenance(), &evaluation) !=
        CANDIDATE_EVALUATION_OK) {
        return CANDIDATE_EVALUATOR_ERR_EVALUATION_FAILED;
    }

    *out = evaluation;
    return CANDIDATE_EVALUATOR_OK;
}

static Candidate *make_candidate(void) {
    Candidate *candidate;
    assert(candidate_create(make_id("candidate-mock"), make_id("structure-candidate-material"), NULL, 0, NULL, 0, 0,
                             1, make_absent_provenance(), &candidate) == CANDIDATE_OK);
    return candidate;
}

int main(void) {
    /* CandidateEvaluation: construction, accessors. */
    Interpretation *evaluations[1] = {make_interpretation("evaluation-1", "stays within vocal range")};
    Interpretation *violations[1] = {make_interpretation("violation-1", "parallel fifths at m.3")};

    CandidateEvaluation *evaluation;
    assert(candidate_evaluation_create(evaluations, 1, violations, 1, 3, 4, make_absent_provenance(), &evaluation) ==
           CANDIDATE_EVALUATION_OK);
    assert(candidate_evaluation_evaluation_count(evaluation) == 1);
    assert(strcmp(interpretation_author(candidate_evaluation_evaluation_at(evaluation, 0)), "mock-evaluator") == 0);
    assert(candidate_evaluation_violation_count(evaluation) == 1);
    assert(candidate_evaluation_score_numerator(evaluation) == 3);
    assert(candidate_evaluation_score_denominator(evaluation) == 4);
    assert(provenance_reference_state(candidate_evaluation_provenance(evaluation)) == PROVENANCE_REFERENCE_ABSENT);

    /* Empty CandidateEvaluation: no findings either way is still valid. */
    CandidateEvaluation *empty;
    assert(candidate_evaluation_create(NULL, 0, NULL, 0, 0, 1, make_absent_provenance(), &empty) ==
           CANDIDATE_EVALUATION_OK);
    assert(candidate_evaluation_evaluation_count(empty) == 0);
    assert(candidate_evaluation_violation_count(empty) == 0);

    /* Rejects a zero score denominator. */
    ProvenanceReference *rejected_provenance = make_absent_provenance();
    CandidateEvaluation *rejected;
    assert(candidate_evaluation_create(NULL, 0, NULL, 0, 1, 0, rejected_provenance, &rejected) ==
           CANDIDATE_EVALUATION_ERR_NON_POSITIVE_SCORE_DENOMINATOR);
    provenance_reference_destroy(rejected_provenance);

    /* CandidateEvaluator interface contract: a concrete evaluator is name + CandidateEvaluatorFn. */
    CandidateEvaluator mock = {"mock-evaluator", mock_evaluate};
    assert(strcmp(mock.name, "mock-evaluator") == 0);

    /* NULL candidate is rejected without touching *out. */
    CandidateEvaluation *untouched = (CandidateEvaluation *)0x1;
    assert(mock.evaluate(NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0, &untouched) ==
           CANDIDATE_EVALUATOR_ERR_NULL_CANDIDATE);
    assert(untouched == (CandidateEvaluation *)0x1);

    /* A valid candidate, judged against borrowed L7 vocabulary, always yields the same explainable result. */
    Candidate *candidate = make_candidate();
    Framework *framework;
    assert(framework_create(make_id("framework-generation-test"), "generation-test", &framework) == FRAMEWORK_OK);
    Rule *rule;
    assert(rule_create(make_id("rule-1"), framework, NULL, &rule) == RULE_OK);
    Constraint *constraint;
    assert(constraint_create(make_id("constraint-1"), framework, NULL, CONSTRAINT_TYPE_HARD, 0, 0, NULL,
                              &constraint) == CONSTRAINT_OK);
    Preference *preference;
    assert(preference_create(make_id("preference-1"), framework, &preference) == PREFERENCE_OK);
    StatisticalModel *statistical_model;
    long feature_content = 1;
    long output_content = 1;
    Value *input_features;
    assert(value_create("features", &feature_content, sizeof(feature_content), &input_features) == VALUE_OK);
    Value *output;
    assert(value_create("output", &output_content, sizeof(output_content), &output) == VALUE_OK);
    assert(statistical_model_create(make_id("statistical-model-1"), input_features, output, 1, 2, "test-corpus",
                                     "1.0.0", NULL, &statistical_model) == STATISTICAL_MODEL_OK);

    const Rule *rules[1] = {rule};
    const Constraint *constraints[1] = {constraint};
    const Preference *preferences[1] = {preference};
    const StatisticalModel *statistical_models[1] = {statistical_model};

    CandidateEvaluation *result;
    assert(mock.evaluate(candidate, rules, 1, constraints, 1, preferences, 1, statistical_models, 1, &result) ==
           CANDIDATE_EVALUATOR_OK);
    assert(candidate_evaluation_evaluation_count(result) == 1);
    assert(candidate_evaluation_violation_count(result) == 1);
    assert(candidate_evaluation_score_numerator(result) == 3);
    assert(candidate_evaluation_score_denominator(result) == 4);

    candidate_evaluation_destroy(evaluation);
    candidate_evaluation_destroy(empty);
    candidate_evaluation_destroy(result);
    candidate_destroy(candidate);
    rule_destroy(rule);
    constraint_destroy(constraint);
    preference_destroy(preference);
    statistical_model_destroy(statistical_model);
    framework_destroy(framework);

    printf("test_candidate_evaluator: ok\n");
    return 0;
}