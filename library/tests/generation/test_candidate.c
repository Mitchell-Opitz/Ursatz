#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "generation/candidate.h"
#include "generation/generator.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Construction test: Candidate is a proposed result pre-acceptance --
 * material referenced by id, evaluations/violations as independent
 * Interpretation arrays, an exact-rational score, and provenance.
 *
 * CandidateSet integration test: this is also the type generator.h
 * forward-declares as an opaque `struct CandidateSet` without defining
 * it -- constructing one here and handing it back through a Generator's
 * GeneratorFn return path proves both headers resolve to the same
 * type, not two conflicting ones.
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
    assert(value_create("CandidateClaim", claim_content, strlen(claim_content) + 1, &claim) == VALUE_OK);
    Uncertainty *uncertainty;
    assert(uncertainty_create_unknown(&uncertainty) == UNCERTAINTY_OK);
    Interpretation *interpretation;
    assert(interpretation_create(make_id(id_value), make_id("target-mock"), claim, NULL, NULL, NULL, uncertainty,
                                  "mock-evaluator", "mock-method", INTERPRETATION_STATUS_MACHINE_GENERATED,
                                  make_absent_provenance(), &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

static Candidate *make_candidate(const char *id_value) {
    Interpretation *evaluations[1] = {make_interpretation("evaluation-1", "stays within vocal range")};
    Candidate *candidate;
    assert(candidate_create(make_id(id_value), make_id("structure-candidate-material"), evaluations, 1, NULL, 0, 3, 4,
                             make_absent_provenance(), &candidate) == CANDIDATE_OK);
    return candidate;
}

static GeneratorResult mock_generate(const GenerationPlan *plan, unsigned long random_seed, const Value *configuration,
                                      CandidateSet **out) {
    (void)plan;
    (void)configuration;

    Candidate *candidates[1] = {make_candidate("candidate-mock-1")};
    EntityID *input_ids[1] = {make_id("intent-mock")};
    CandidateSet *set;
    if (candidate_set_create(make_id("candidate-set-mock"), candidates, 1, random_seed, "1.0.0", NULL, input_ids, 1,
                              NULL, make_absent_provenance(), &set) != CANDIDATE_SET_OK) {
        return GENERATOR_ERR_GENERATION_FAILED;
    }

    *out = set;
    return GENERATOR_OK;
}

int main(void) {
    /* Candidate: construction, accessors. */
    Interpretation *evaluations[2] = {make_interpretation("evaluation-vocal-range", "stays within vocal range"),
                                       make_interpretation("evaluation-stepwise", "mostly stepwise motion")};
    Interpretation *violations[1] = {make_interpretation("violation-parallel-fifths", "parallel fifths at m.3")};

    EntityID *id = make_id("candidate-1");
    EntityID *material_id = make_id("structure-candidate-material");
    Candidate *candidate;
    assert(candidate_create(id, material_id, evaluations, 2, violations, 1, 3, 4, make_absent_provenance(),
                             &candidate) == CANDIDATE_OK);

    assert(entity_id_equals(candidate_material_id(candidate), material_id));
    assert(candidate_evaluation_count(candidate) == 2);
    assert(strcmp(interpretation_author(candidate_evaluation_at(candidate, 0)), "mock-evaluator") == 0);
    assert(candidate_violation_count(candidate) == 1);
    assert(candidate_score_numerator(candidate) == 3);
    assert(candidate_score_denominator(candidate) == 4);

    /* Empty Candidate: not yet evaluated is still valid. */
    Candidate *unevaluated;
    assert(candidate_create(make_id("candidate-unevaluated"), make_id("structure-unevaluated"), NULL, 0, NULL, 0, 0,
                             1, make_absent_provenance(), &unevaluated) == CANDIDATE_OK);
    assert(candidate_evaluation_count(unevaluated) == 0);
    assert(candidate_violation_count(unevaluated) == 0);

    assert(candidate_identity_equals(candidate, candidate));
    assert(!candidate_identity_equals(candidate, unevaluated));

    /* Rejects a zero score denominator. */
    EntityID *bad_score_id = make_id("candidate-rejects-zero-score-denominator");
    EntityID *bad_score_material_id = make_id("structure-bad-score");
    ProvenanceReference *bad_score_provenance = make_absent_provenance();
    Candidate *rejected_score;
    assert(candidate_create(bad_score_id, bad_score_material_id, NULL, 0, NULL, 0, 1, 0, bad_score_provenance,
                             &rejected_score) == CANDIDATE_ERR_NON_POSITIVE_SCORE_DENOMINATOR);
    entity_id_destroy(bad_score_id);
    entity_id_destroy(bad_score_material_id);
    provenance_reference_destroy(bad_score_provenance);

    /* CandidateSet: construction, accessors, TDD Sec163 reproducibility fields. */
    Candidate *set_candidates[1] = {make_candidate("candidate-in-set")};
    EntityID *input_ids[2] = {make_id("generation-plan-mock"), make_id("intent-mock")};
    EntityID *set_id = make_id("candidate-set-1");
    CandidateSet *set;
    assert(candidate_set_create(set_id, set_candidates, 1, 42, "1.0.0", NULL, input_ids, 2, NULL,
                                 make_absent_provenance(), &set) == CANDIDATE_SET_OK);

    assert(candidate_set_candidate_count(set) == 1);
    assert(candidate_set_random_seed(set) == 42);
    assert(strcmp(candidate_set_generator_version(set), "1.0.0") == 0);
    assert(candidate_set_configuration(set) == NULL);
    assert(candidate_set_input_id_count(set) == 2);
    assert(candidate_set_framework_version(set) == NULL);

    /* Empty CandidateSet: a run that produced no viable candidates is still valid. */
    CandidateSet *empty_set;
    assert(candidate_set_create(make_id("candidate-set-empty"), NULL, 0, 7, "1.0.0", NULL, NULL, 0, NULL,
                                 make_absent_provenance(), &empty_set) == CANDIDATE_SET_OK);
    assert(candidate_set_candidate_count(empty_set) == 0);

    /* Rejects a missing generator_version. */
    EntityID *no_version_id = make_id("candidate-set-rejects-no-version");
    ProvenanceReference *no_version_provenance = make_absent_provenance();
    CandidateSet *rejected_no_version;
    assert(candidate_set_create(no_version_id, NULL, 0, 0, NULL, NULL, NULL, 0, NULL, no_version_provenance,
                                 &rejected_no_version) == CANDIDATE_SET_ERR_NULL_GENERATOR_VERSION);
    entity_id_destroy(no_version_id);
    provenance_reference_destroy(no_version_provenance);

    /* Generator integration: generator.h's forward-declared CandidateSet resolves to this same type. */
    Generator mock = {"mock-generator", "GenerationPlan", "CandidateSet", "none", "none", false, "1.0.0",
                       mock_generate};
    CandidateSet *generated;
    assert(mock.generate(NULL, 99, NULL, &generated) == GENERATOR_OK);
    assert(candidate_set_random_seed(generated) == 99);
    assert(candidate_set_candidate_count(generated) == 1);
    assert(candidate_score_numerator(candidate_set_candidate_at(generated, 0)) == 3);

    candidate_destroy(candidate);
    candidate_destroy(unevaluated);
    candidate_set_destroy(set);
    candidate_set_destroy(empty_set);
    candidate_set_destroy(generated);

    printf("test_candidate: ok\n");
    return 0;
}