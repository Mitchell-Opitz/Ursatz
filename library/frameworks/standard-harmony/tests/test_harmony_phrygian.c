#include <assert.h>
#include <stdio.h>

#include "harmony_phrygian.h"
#include "harmony_test_util.h"

static bool is_phc(HarmonyMode mode, HarmonyChord a, HarmonyChord b) {
    HarmonyChord chords[2] = {a, b};
    return harmony_phrygian_half_cadence_is(mode, chords);
}

static void test_iv6_to_v_in_minor(void) {
    assert(is_phc(HARMONY_MODE_MINOR, CHI(4, 1), CH(5)));
    assert(is_phc(HARMONY_MODE_MINOR, CHI(4, 1), CH7(5)));
}

static void test_it_occurs_only_in_minor(void) {
    assert(!is_phc(HARMONY_MODE_MAJOR, CHI(4, 1), CH(5)));
}

static void test_other_pairs_are_not_phrygian(void) {
    assert(!is_phc(HARMONY_MODE_MINOR, CH(4), CH(5)));
    assert(!is_phc(HARMONY_MODE_MINOR, CHI(4, 2), CH(5)));
    assert(!is_phc(HARMONY_MODE_MINOR, CH7I(4, 1), CH(5)));
    assert(!is_phc(HARMONY_MODE_MINOR, CHI(2, 1), CH(5)));
    assert(!is_phc(HARMONY_MODE_MINOR, CHI(4, 1), CHI(5, 1)));
    assert(!is_phc(HARMONY_MODE_MINOR, CHI(4, 1), CH(1)));
    assert(!is_phc(HARMONY_MODE_MINOR, CHI(4, 1), CADENTIAL_CHORD));
}

static void test_invalid_subjects(void) {
    HarmonyChord good[2] = {CHI(4, 1), CH(5)};
    HarmonyChord bad[2] = {CHI(4, 1), CH(9)};
    assert(!harmony_phrygian_half_cadence_is(HARMONY_MODE_MINOR, NULL));
    assert(!harmony_phrygian_half_cadence_is(HARMONY_MODE_COUNT, good));
    assert(!harmony_phrygian_half_cadence_is(HARMONY_MODE_MINOR, bad));
}

int main(void) {
    test_iv6_to_v_in_minor();
    test_it_occurs_only_in_minor();
    test_other_pairs_are_not_phrygian();
    test_invalid_subjects();
    printf("test_harmony_phrygian: all tests passed\n");
    return 0;
}
