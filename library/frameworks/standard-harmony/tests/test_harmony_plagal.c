#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_plagal.h"
#include "harmony_test_util.h"

#define AFTER HARMONY_PLAGAL_AFTER_CADENCE
#define BEGIN HARMONY_PLAGAL_PHRASE_BEGINNING

static bool is_plagal(HarmonyChord a, HarmonyChord b, HarmonyPlagalPlacement placement) {
    HarmonyChord chords[2] = {a, b};
    return harmony_plagal_progression_is(chords, placement);
}

static void test_placements(void) {
    assert(harmony_plagal_placement_valid(AFTER));
    assert(harmony_plagal_placement_valid(BEGIN));
    assert(!harmony_plagal_placement_valid(HARMONY_PLAGAL_PLACEMENT_COUNT));
    assert(!harmony_plagal_placement_valid((HarmonyPlagalPlacement)-1));
    assert(strcmp(harmony_plagal_placement_name(AFTER), "after-cadence") == 0);
    assert(strcmp(harmony_plagal_placement_name(BEGIN), "phrase-beginning") == 0);
    assert(harmony_plagal_placement_name(HARMONY_PLAGAL_PLACEMENT_COUNT) == NULL);
}

static void test_iv_to_root_position_i_in_both_placements(void) {
    assert(is_plagal(CH(4), CH(1), AFTER));
    assert(is_plagal(CH(4), CH(1), BEGIN));
}

static void test_iv_to_i6_only_at_a_phrase_beginning(void) {
    assert(is_plagal(CH(4), CHI(1, 1), BEGIN));
    assert(!is_plagal(CH(4), CHI(1, 1), AFTER));
}

static void test_other_pairs_are_not_plagal(void) {
    assert(!is_plagal(CH(5), CH(1), AFTER));
    assert(!is_plagal(CH(2), CH(1), AFTER));
    assert(!is_plagal(CH(1), CH(4), AFTER));
    assert(!is_plagal(CH(4), CH(5), AFTER));
    assert(!is_plagal(CH(4), CH(6), BEGIN));
    assert(!is_plagal(CHI(4, 1), CH(1), BEGIN));
    assert(!is_plagal(CHI(4, 2), CH(1), BEGIN));
    assert(!is_plagal(CH(4), CHI(1, 2), BEGIN));
    assert(!is_plagal(CH7(4), CH(1), AFTER));
    assert(!is_plagal(CH(4), CH7(1), AFTER));
    assert(!is_plagal(CH(4), CADENTIAL_CHORD, AFTER));
}

static void test_invalid_subjects(void) {
    HarmonyChord good[2] = {CH(4), CH(1)};
    HarmonyChord bad[2] = {CH(4), CH(9)};
    assert(!harmony_plagal_progression_is(NULL, AFTER));
    assert(!harmony_plagal_progression_is(good, HARMONY_PLAGAL_PLACEMENT_COUNT));
    assert(!harmony_plagal_progression_is(bad, AFTER));
}

int main(void) {
    test_placements();
    test_iv_to_root_position_i_in_both_placements();
    test_iv_to_i6_only_at_a_phrase_beginning();
    test_other_pairs_are_not_plagal();
    test_invalid_subjects();
    printf("test_harmony_plagal: all tests passed\n");
    return 0;
}
