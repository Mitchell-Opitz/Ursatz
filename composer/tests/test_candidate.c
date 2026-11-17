#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <model/core/provenance_reference.h>
#include <model/core/value.h>
#include <model/identity/entity_id.h>
#include <model/interpretation/interpretation.h>
#include <model/interpretation/uncertainty.h>

#include <ursatz_composer/candidate.h>

/*
 * Construction test: Candidate is a proposed result pre-acceptance --
 * material referenced by id, evaluations/violations as independent
 * Interpretation arrays, an exact-rational score, and provenance.
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
    assert(uncertainty_create_scalar_confidence(1, 1, &uncertainty) == UNCERTAINTY_OK);
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

int main(void) {
    /* Candidate: construction, score accessors. */
    Interpretation *evaluations[2] = {make_interpretation("evaluation-vocal-range", "stays within vocal range"),
                                       make_interpretation("evaluation-stepwise", "mostly stepwise motion")};
    Interpretation *violations[1] = {make_interpretation("violation-parallel-fifths", "parallel fifths at m.3")};

    EntityID *id = make_id("candidate-1");
    EntityID *material_id = make_id("structure-candidate-material");
    Candidate *candidate;
    assert(candidate_create(id, material_id, evaluations, 2, violations, 1, 3, 4, make_absent_provenance(),
                             &candidate) == CANDIDATE_OK);

    assert(candidate_score_numerator(candidate) == 3);
    assert(candidate_score_denominator(candidate) == 4);

    /* Empty Candidate: not yet evaluated is still valid. */
    Candidate *unevaluated;
    assert(candidate_create(make_id("candidate-unevaluated"), make_id("structure-unevaluated"), NULL, 0, NULL, 0, 0,
                             1, make_absent_provenance(), &unevaluated) == CANDIDATE_OK);

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

    /* CandidateSet: construction, the candidates it holds, TDD Sec163 reproducibility fields. */
    Candidate *set_candidates[1] = {make_candidate("candidate-in-set")};
    EntityID *input_ids[2] = {make_id("generation-plan-mock"), make_id("intent-mock")};
    EntityID *set_id = make_id("candidate-set-1");
    CandidateSet *set;
    assert(candidate_set_create(set_id, set_candidates, 1, 42, "1.0.0", NULL, input_ids, 2, NULL,
                                 make_absent_provenance(), &set) == CANDIDATE_SET_OK);


    assert(candidate_score_numerator(candidate_set_candidate_at(set, 0)) == 3);

    /* Empty CandidateSet: a run that produced no viable candidates is still valid. */
    CandidateSet *empty_set;
    assert(candidate_set_create(make_id("candidate-set-empty"), NULL, 0, 7, "1.0.0", NULL, NULL, 0, NULL,
                                 make_absent_provenance(), &empty_set) == CANDIDATE_SET_OK);

    /* Rejects a missing generator_version. */
    EntityID *no_version_id = make_id("candidate-set-rejects-no-version");
    ProvenanceReference *no_version_provenance = make_absent_provenance();
    CandidateSet *rejected_no_version;
    assert(candidate_set_create(no_version_id, NULL, 0, 0, NULL, NULL, NULL, 0, NULL, no_version_provenance,
                                 &rejected_no_version) == CANDIDATE_SET_ERR_NULL_GENERATOR_VERSION);
    entity_id_destroy(no_version_id);
    provenance_reference_destroy(no_version_provenance);

    candidate_destroy(candidate);
    candidate_destroy(unevaluated);
    candidate_set_destroy(set);
    candidate_set_destroy(empty_set);

    printf("test_candidate: ok\n");
    return 0;
}