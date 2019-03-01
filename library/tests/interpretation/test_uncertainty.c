#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "interpretation/uncertainty.h"

/*
 * Invariant test: "unknown", "known approximately" (scalar confidence),
 * and "known candidates with probabilities" (candidate set) MUST
 * remain distinguishable -- constructing each produces a different
 * uncertainty_kind, and none of the three collapses into another
 * (TDD §86).
 */

int main(void) {
    Uncertainty *unknown;
    assert(uncertainty_create_unknown(&unknown) == UNCERTAINTY_OK);
    assert(uncertainty_kind(unknown) == UNCERTAINTY_KIND_UNKNOWN);

    Uncertainty *scalar;
    assert(uncertainty_create_scalar_confidence(3, 4, &scalar) == UNCERTAINTY_OK);
    assert(uncertainty_kind(scalar) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(scalar) == 3);
    assert(uncertainty_scalar_confidence_denominator(scalar) == 4);

    UncertaintyCandidate candidates[2] = {
        {"major", 3, 5},
        {"minor", 2, 5},
    };
    Uncertainty *candidate_set;
    assert(uncertainty_create_candidate_set(candidates, 2, &candidate_set) == UNCERTAINTY_OK);
    assert(uncertainty_kind(candidate_set) == UNCERTAINTY_KIND_CANDIDATE_SET);
    assert(uncertainty_candidate_count(candidate_set) == 2);
    assert(strcmp(uncertainty_candidate_label_at(candidate_set, 0), "major") == 0);
    assert(uncertainty_candidate_weight_numerator_at(candidate_set, 0) == 3);
    assert(uncertainty_candidate_weight_denominator_at(candidate_set, 0) == 5);
    assert(strcmp(uncertainty_candidate_label_at(candidate_set, 1), "minor") == 0);

    /* The three kinds are pairwise distinct -- none collapses into another. */
    assert(uncertainty_kind(unknown) != uncertainty_kind(scalar));
    assert(uncertainty_kind(unknown) != uncertainty_kind(candidate_set));
    assert(uncertainty_kind(scalar) != uncertainty_kind(candidate_set));

    uncertainty_destroy(unknown);
    uncertainty_destroy(scalar);
    uncertainty_destroy(candidate_set);

    /* A confidence outside [0, 1] or a zero denominator is rejected. */
    Uncertainty *rejected;
    assert(uncertainty_create_scalar_confidence(5, 4, &rejected) == UNCERTAINTY_ERR_CONFIDENCE_OUT_OF_RANGE);
    assert(uncertainty_create_scalar_confidence(1, 0, &rejected) == UNCERTAINTY_ERR_NON_POSITIVE_DENOMINATOR);

    /* An empty candidate set is rejected -- it is not a valid "known candidates" shape. */
    Uncertainty *empty_candidates;
    assert(uncertainty_create_candidate_set(NULL, 0, &empty_candidates) == UNCERTAINTY_ERR_NULL_CANDIDATES);

    printf("test_uncertainty: ok\n");
    return 0;
}