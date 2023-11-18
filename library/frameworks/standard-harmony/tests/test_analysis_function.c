#include <math.h>

#include "analysis_test_util.h"

/* Harmonic function per segment, and the neutral input's reading of it. */

static void check(Fixture *f, size_t index, HarmonicFunction expected, bool applied, int applied_to) {
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f->inputs, f->count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyFunctionResult *results = NULL;
    assert(harmony_analysis_assign_functions(segments, f->count, &results) == HARMONY_FUNCTION_OK);
    assert(results[index].has_function && results[index].function == expected);
    assert(results[index].applied == applied && results[index].applied_to == applied_to);

    HarmonyFunctionClaim claim;
    assert(harmony_function_claim_read(results[index].interpretation, &claim));
    assert(claim.function == (int)expected && claim.applied == applied && claim.applied_to == applied_to);
    harmony_function_results_destroy(results, f->count);
    harmony_analysis_destroy(segments);
}

static void test_diatonic_functions(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);                                  /* I */
    fixture_add(&f, 2, TRIAD_MINOR, 3, 1, 4, 6);                                  /* ii6 */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);                                  /* IV */
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7);                             /* V7 */
    fixture_add(&f, 6, TRIAD_MINOR, 3, 0, 6, 1);                                  /* vi */
    fixture_add(&f, 7, TRIAD_DIM, 3, 1, 2, 4);                                    /* vii-diminished 6 */
    check(&f, 0, HARMONIC_FUNCTION_TONIC, false, 0);
    check(&f, 1, HARMONIC_FUNCTION_PREDOMINANT, false, 0);
    check(&f, 2, HARMONIC_FUNCTION_PREDOMINANT, false, 0);
    check(&f, 3, HARMONIC_FUNCTION_DOMINANT, false, 0);
    check(&f, 4, HARMONIC_FUNCTION_PREDOMINANT, false, 0); /* weak predominant */
    check(&f, 5, HARMONIC_FUNCTION_DOMINANT, false, 0);
    fixture_destroy(&f);
}

static void test_applied_chord_is_dominant_toward_its_target(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 2, 4)->applied_to = 5;                  /* V/V */
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 1, 5, 7)->applied_to = 4;             /* V6/5 of IV */
    fixture_add(&f, 7, TRIAD_DIM, 3, 0, 4, 1)->applied_to = 5;                    /* vii-diminished of V */
    check(&f, 0, HARMONIC_FUNCTION_DOMINANT, true, 5);
    check(&f, 1, HARMONIC_FUNCTION_DOMINANT, true, 4);
    check(&f, 2, HARMONIC_FUNCTION_DOMINANT, true, 5);

    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    assert(segments[0].applied && segments[0].applied_chord.kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT);
    assert(segments[2].applied && segments[2].applied_chord.kind == APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_minor_key_leading_tone(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 7, TRIAD_DIM, 3, 0, 7, 2)->mode = HARMONY_MODE_MINOR;   /* vii-diminished on the raised 7th */
    fixture_add(&f, 7, TRIAD_MAJOR, 3, 0, 7, 2)->mode = HARMONY_MODE_MINOR; /* the subtonic VII */
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    assert(segments[0].valid && segments[0].chord.leading_tone);
    assert(segments[1].valid && !segments[1].chord.leading_tone);
    harmony_analysis_destroy(segments);
    check(&f, 0, HARMONIC_FUNCTION_DOMINANT, false, 0);
    check(&f, 1, HARMONIC_FUNCTION_PREDOMINANT, false, 0); /* VII is a weak predominant */
    fixture_destroy(&f);
}

static void test_uncertainty_and_framework_carry_through(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7)->uncertainty = 0.25;
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyFunctionResult *results = NULL;
    assert(harmony_analysis_assign_functions(segments, f.count, &results) == HARMONY_FUNCTION_OK);
    const Interpretation *interpretation = results[0].interpretation;
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "seg-0") == 0);
    assert(interpretation_framework_reference(interpretation) != NULL);
    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 750000UL);
    assert(fabs(results[0].uncertainty - 0.25) < 1e-12);
    harmony_function_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_segments_that_are_not_chords_pass_through(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 9, TRIAD_MAJOR, 3, 0, 1, 1);                    /* degree out of range */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 3, 1, 1);                    /* a triad has no third inversion */
    int odd[3] = {0, 2, 7};
    fixture_add(&f, 1, odd, 3, 0, 1, 1);                            /* an interval set with no name */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1)->applied_to = 1;    /* the tonic is not a target */
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyFunctionResult *results = NULL;
    assert(harmony_analysis_assign_functions(segments, f.count, &results) == HARMONY_FUNCTION_OK);
    for (size_t i = 0; i < f.count; i++) {
        assert(!segments[i].valid && !results[i].has_function && results[i].interpretation == NULL);
    }
    harmony_function_results_destroy(results, f.count);
    harmony_analysis_destroy(segments);

    f.inputs[0].segment_id = "";
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_ERR_BAD_INPUT);
    fixture_destroy(&f);
}

int main(void) {
    test_diatonic_functions();
    test_applied_chord_is_dominant_toward_its_target();
    test_minor_key_leading_tone();
    test_uncertainty_and_framework_carry_through();
    test_segments_that_are_not_chords_pass_through();
    printf("standard-harmony analysis function: all tests passed\n");
    return 0;
}
