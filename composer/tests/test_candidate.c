#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <model/core/provenance_reference.h>
#include <model/core/value.h>
#include <model/identity/entity_id.h>

#include <ursatz_composer/candidate.h>

/*
 * Construction test: Candidate is a proposed result pre-acceptance --
 * material referenced by id, an exact-rational score, and provenance.
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

static Candidate *make_candidate(const char *id_value) {
    Candidate *candidate;
    assert(candidate_create(make_id(id_value), make_id("structure-candidate-material"), 3, 4,
                             make_absent_provenance(), &candidate) == CANDIDATE_OK);
    return candidate;
}

int main(void) {
    /* Candidate: construction, score accessors. */
    EntityID *id = make_id("candidate-1");
    EntityID *material_id = make_id("structure-candidate-material");
    Candidate *candidate;
    assert(candidate_create(id, material_id, 3, 4, make_absent_provenance(), &candidate) == CANDIDATE_OK);

    assert(candidate_score_numerator(candidate) == 3);
    assert(candidate_score_denominator(candidate) == 4);

    /* Empty Candidate: not yet evaluated is still valid. */
    Candidate *unevaluated;
    assert(candidate_create(make_id("candidate-unevaluated"), make_id("structure-unevaluated"), 0, 1,
                             make_absent_provenance(), &unevaluated) == CANDIDATE_OK);

    /* Rejects a zero score denominator. */
    EntityID *bad_score_id = make_id("candidate-rejects-zero-score-denominator");
    EntityID *bad_score_material_id = make_id("structure-bad-score");
    ProvenanceReference *bad_score_provenance = make_absent_provenance();
    Candidate *rejected_score;
    assert(candidate_create(bad_score_id, bad_score_material_id, 1, 0, bad_score_provenance,
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