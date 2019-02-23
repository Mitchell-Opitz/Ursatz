#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "interpretation/hypothesis.h"

/*
 * Context test: multiple simultaneous Hypotheses can reference the
 * same Observation without conflict -- constructing two independent
 * Hypotheses that both reference the same observation EntityID
 * succeeds for both, and each retains its own distinct candidate set.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *shared_observation = make_id("observation-shared");

    EntityID *hypothesis_a_id = make_id("hypothesis-a");
    EntityID *hypothesis_a_observations[1] = {make_id("observation-shared")};
    HypothesisCandidate hypothesis_a_candidates[2] = {
        {"dominant-seventh", 3, 5},
        {"passing-tone", 2, 5},
    };
    Hypothesis *hypothesis_a;
    assert(hypothesis_create(hypothesis_a_id, hypothesis_a_observations, 1, NULL, 0, hypothesis_a_candidates, 2,
                              &hypothesis_a) == HYPOTHESIS_OK);

    EntityID *hypothesis_b_id = make_id("hypothesis-b");
    EntityID *hypothesis_b_observations[1] = {make_id("observation-shared")};
    HypothesisCandidate hypothesis_b_candidates[1] = {
        {"suspension", 1, 1},
    };
    Hypothesis *hypothesis_b;
    assert(hypothesis_create(hypothesis_b_id, hypothesis_b_observations, 1, NULL, 0, hypothesis_b_candidates, 1,
                              &hypothesis_b) == HYPOTHESIS_OK);

    /* Both Hypotheses reference the same Observation without conflict. */
    assert(entity_id_equals(hypothesis_observation_id_at(hypothesis_a, 0), shared_observation));
    assert(entity_id_equals(hypothesis_observation_id_at(hypothesis_b, 0), shared_observation));

    /* Multiple candidates: a Hypothesis is not a single value with just a
       confidence score. */
    assert(hypothesis_candidate_count(hypothesis_a) == 2);
    assert(strcmp(hypothesis_candidate_label_at(hypothesis_a, 0), "dominant-seventh") == 0);
    assert(strcmp(hypothesis_candidate_label_at(hypothesis_a, 1), "passing-tone") == 0);

    /* Each Hypothesis retains its own independent candidate set. */
    assert(hypothesis_candidate_count(hypothesis_b) == 1);
    assert(strcmp(hypothesis_candidate_label_at(hypothesis_b, 0), "suspension") == 0);

    hypothesis_destroy(hypothesis_a);
    hypothesis_destroy(hypothesis_b);
    entity_id_destroy(shared_observation);

    printf("test_hypothesis: ok\n");
    return 0;
}