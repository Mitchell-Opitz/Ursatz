#include <assert.h>
#include <stdio.h>

#include "harmony_applied.h"

/* An applied chord has dominant function, toward its target. */

static void test_every_valid_applied_chord_is_dominant(void) {
    HarmonicFunction function;
    HarmonyAppliedChord v_of_v = {APPLIED_CHORD_KIND_SECONDARY_DOMINANT, HARMONY_APPLIED_MAJOR_TRIAD, 5};
    assert(harmony_applied_function(HARMONY_MODE_MAJOR, &v_of_v, &function) && function == HARMONIC_FUNCTION_DOMINANT);
    HarmonyAppliedChord v7_of_iv = {APPLIED_CHORD_KIND_SECONDARY_DOMINANT, HARMONY_APPLIED_DOMINANT_SEVENTH, 4};
    assert(harmony_applied_function(HARMONY_MODE_MAJOR, &v7_of_iv, &function) &&
           function == HARMONIC_FUNCTION_DOMINANT);
    HarmonyAppliedChord vii_of_v = {APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE, HARMONY_APPLIED_DIMINISHED_TRIAD, 5};
    assert(harmony_applied_function(HARMONY_MODE_MAJOR, &vii_of_v, &function) &&
           function == HARMONIC_FUNCTION_DOMINANT);
    HarmonyAppliedChord v7_of_iv_minor = {APPLIED_CHORD_KIND_SECONDARY_DOMINANT, HARMONY_APPLIED_DOMINANT_SEVENTH, 4};
    assert(harmony_applied_function(HARMONY_MODE_MINOR, &v7_of_iv_minor, &function) &&
           function == HARMONIC_FUNCTION_DOMINANT);
}

static void test_invalid_is_rejected(void) {
    HarmonicFunction function = HARMONIC_FUNCTION_TONIC;
    HarmonyAppliedChord tonic_target = {APPLIED_CHORD_KIND_SECONDARY_DOMINANT, HARMONY_APPLIED_MAJOR_TRIAD, 1};
    assert(!harmony_applied_function(HARMONY_MODE_MAJOR, &tonic_target, &function));
    HarmonyAppliedChord mismatched = {APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE, HARMONY_APPLIED_MAJOR_TRIAD, 5};
    assert(!harmony_applied_function(HARMONY_MODE_MAJOR, &mismatched, &function));
    assert(!harmony_applied_function(HARMONY_MODE_MAJOR, NULL, &function));
    assert(!harmony_applied_function(HARMONY_MODE_MAJOR, &tonic_target, NULL));
    assert(function == HARMONIC_FUNCTION_TONIC);
}

int main(void) {
    test_every_valid_applied_chord_is_dominant();
    test_invalid_is_rejected();
    printf("standard-harmony applied function: all tests passed\n");
    return 0;
}
