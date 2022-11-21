#include <assert.h>
#include <stdio.h>

#include "harmony_test_util.h"
#include "harmony_tonicized_deceptive.h"

#define SD APPLIED_CHORD_KIND_SECONDARY_DOMINANT
#define SLT APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE

static bool is(HarmonyMode mode, HarmonyChord dominant, HarmonyAppliedChord applied, HarmonyChord resolution) {
    return harmony_tonicized_deceptive_is(mode, &dominant, &applied, &resolution);
}

static void test_v_then_viio7_of_vi_then_vi(void) {
    HarmonyAppliedChord viio7_of_vi = {SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 6};
    HarmonyAppliedChord viio_of_vi = {SLT, HARMONY_APPLIED_DIMINISHED_TRIAD, 6};
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        assert(is(m, CH(5), viio7_of_vi, CH(6)));
        assert(is(m, CH7(5), viio7_of_vi, CH(6)));
        assert(is(m, CHI(5, 1), viio_of_vi, CH(6)));
    }
}

static void test_other_chords_are_not_tonicized_deceptive_motion(void) {
    HarmonyAppliedChord viio7_of_vi = {SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 6};
    HarmonyAppliedChord viio7_of_v = {SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 5};
    HarmonyAppliedChord v7_of_vi = {SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 6};
    assert(!is(HARMONY_MODE_MAJOR, CH(4), viio7_of_vi, CH(6)));
    assert(!is(HARMONY_MODE_MAJOR, CADENTIAL_CHORD, viio7_of_vi, CH(6)));
    assert(!is(HARMONY_MODE_MAJOR, CH(5), viio7_of_v, CH(6)));
    assert(!is(HARMONY_MODE_MAJOR, CH(5), v7_of_vi, CH(6)));
    assert(!is(HARMONY_MODE_MAJOR, CH(5), viio7_of_vi, CH(1)));
    assert(!is(HARMONY_MODE_MAJOR, CH(5), viio7_of_vi, CHI(6, 1)));
    assert(!is(HARMONY_MODE_MAJOR, CH(5), viio7_of_vi, CH7(6)));
}

static void test_invalid_subjects(void) {
    HarmonyChord dominant = CH(5);
    HarmonyChord resolution = CH(6);
    HarmonyAppliedChord good = {SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 6};
    HarmonyAppliedChord bad = {SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 1};
    assert(!harmony_tonicized_deceptive_is(HARMONY_MODE_MAJOR, NULL, &good, &resolution));
    assert(!harmony_tonicized_deceptive_is(HARMONY_MODE_MAJOR, &dominant, NULL, &resolution));
    assert(!harmony_tonicized_deceptive_is(HARMONY_MODE_MAJOR, &dominant, &good, NULL));
    assert(!harmony_tonicized_deceptive_is(HARMONY_MODE_COUNT, &dominant, &good, &resolution));
    assert(!harmony_tonicized_deceptive_is(HARMONY_MODE_MAJOR, &dominant, &bad, &resolution));
}

int main(void) {
    test_v_then_viio7_of_vi_then_vi();
    test_other_chords_are_not_tonicized_deceptive_motion();
    test_invalid_subjects();
    printf("test_harmony_tonicized_deceptive: all tests passed\n");
    return 0;
}
