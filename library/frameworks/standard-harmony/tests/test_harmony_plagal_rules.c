#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_plagal_rules.h"
#include "harmony_plagal_test_util.h"
#include "harmony_rule_check.h"

#define PLG(name) HARMONY_PLAGAL_##name
#define BIT(rule) (1u << (rule))

static bool holds(HarmonyPlagalRule rule, const HarmonyPlagalMotion *m) {
    return harmony_plagal_rule_holds(rule, m);
}

static void expect_breaks(const HarmonyPlagalMotion *m, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_plagal_check(m, &h, &s));
    assert(h == hard && s == soft);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_plagal_rule, harmony_plagal_constraint, HARMONY_PLAGAL_RULE_COUNT);
    for (int i = 0; i < HARMONY_PLAGAL_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_plagal_constraint((HarmonyPlagalRule)i));
        assert(type == (i <= PLG(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PLAGAL_RULE_COUNT] = {
        "progression", "chord-notes", "step-or-common-tone", "no-parallels", "soprano-do", "avoid-at-beginning"};
    for (int i = 0; i < HARMONY_PLAGAL_RULE_COUNT; i++) {
        assert(strcmp(harmony_plagal_rule_name((HarmonyPlagalRule)i), expected[i]) == 0);
    }
    assert(harmony_plagal_rule_name(PLG(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_after_a_cadence_breaks_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyPlagalMotion m = example_plagal_after_cadence((HarmonyMode)mode);
        for (int rule = 0; rule < HARMONY_PLAGAL_RULE_COUNT; rule++) {
            assert(holds((HarmonyPlagalRule)rule, &m));
        }
        expect_breaks(&m, 0, 0);
    }
}

static void test_the_phrase_beginning_examples_break_only_the_soft_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyPlagalMotion root = example_plagal_beginning_root((HarmonyMode)mode);
        HarmonyPlagalMotion inverted = example_plagal_beginning_inverted((HarmonyMode)mode);
        expect_breaks(&root, 0, BIT(PLG(AVOID_AT_BEGINNING)));
        expect_breaks(&inverted, 0, BIT(PLG(AVOID_AT_BEGINNING)));
    }
}

static void test_progression(void) {
    HarmonyPlagalMotion i6_after = example_plagal_beginning_inverted(HARMONY_MODE_MAJOR);
    i6_after.placement = HARMONY_PLAGAL_AFTER_CADENCE;
    HarmonyPlagalMotion not_iv = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    not_iv.chords[0] = CH(2);
    assert(!holds(PLG(PROGRESSION), &i6_after));
    assert(!holds(PLG(PROGRESSION), &not_iv));
}

static void test_each_chord_has_its_own_notes_all_three(void) {
    HarmonyPlagalMotion no_fifth = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    no_fifth.steps[1][VOICE_PART_ALTO] = 2; /* sol becomes mi: do mi mi do */
    HarmonyPlagalMotion foreign = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    foreign.steps[1][VOICE_PART_TENOR] = 1; /* re is not in I */
    HarmonyPlagalMotion not_doubled = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    not_doubled.steps[0][VOICE_PART_SOPRANO] = 5; /* la la fa fa: fa is doubled, do missing */
    HarmonyPlagalMotion wrong_bass = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    wrong_bass.steps[1][VOICE_PART_BASS] = -5; /* mi in the bass of a root position I */
    HarmonyPlagalMotion i6_bass_root = example_plagal_beginning_inverted(HARMONY_MODE_MAJOR);
    i6_bass_root.steps[1][VOICE_PART_BASS] = -7; /* do in the bass of I6 */
    assert(!holds(PLG(CHORD_NOTES), &no_fifth));
    assert(!holds(PLG(CHORD_NOTES), &foreign));
    assert(!holds(PLG(CHORD_NOTES), &not_doubled));
    assert(!holds(PLG(CHORD_NOTES), &wrong_bass));
    assert(!holds(PLG(CHORD_NOTES), &i6_bass_root));
}

static void test_upper_voices_move_by_step_or_common_tone_in_root_position(void) {
    HarmonyPlagalMotion leaps = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    leaps.steps[1][VOICE_PART_ALTO] = 2; /* la down to mi: a leap */
    leaps.steps[1][VOICE_PART_TENOR] = 4;
    assert(!holds(PLG(STEP_OR_COMMON_TONE), &leaps));
    expect_breaks(&leaps, BIT(PLG(STEP_OR_COMMON_TONE)), 0);
}

static void test_the_inverted_tonic_has_no_step_rule(void) {
    HarmonyPlagalMotion m = example_plagal_beginning_inverted(HARMONY_MODE_MAJOR);
    assert(m.steps[0][VOICE_PART_TENOR] - m.steps[1][VOICE_PART_TENOR] == 3);
    assert(holds(PLG(STEP_OR_COMMON_TONE), &m));
}

static void test_no_parallels_into_the_inverted_tonic(void) {
    HarmonyPlagalMotion m = example_plagal_parallel_octaves(HARMONY_MODE_MAJOR);
    assert(holds(PLG(CHORD_NOTES), &m));
    assert(!holds(PLG(NO_PARALLELS), &m));
    expect_breaks(&m, BIT(PLG(NO_PARALLELS)), BIT(PLG(AVOID_AT_BEGINNING)));
}

static void test_soprano_do_after_a_cadence(void) {
    HarmonyPlagalMotion la = example_plagal_soprano_la(HARMONY_MODE_MAJOR);
    assert(!holds(PLG(SOPRANO_DO), &la));
    expect_breaks(&la, 0, BIT(PLG(SOPRANO_DO)));
    la.placement = HARMONY_PLAGAL_PHRASE_BEGINNING;
    assert(holds(PLG(SOPRANO_DO), &la));
}

static void test_avoid_at_beginning_always_holds_after_a_cadence(void) {
    HarmonyPlagalMotion after = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    HarmonyPlagalMotion begin = example_plagal_beginning_root(HARMONY_MODE_MAJOR);
    assert(holds(PLG(AVOID_AT_BEGINNING), &after));
    assert(!holds(PLG(AVOID_AT_BEGINNING), &begin));
}

static void test_invalid_subject(void) {
    HarmonyPlagalMotion bad_chord = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    bad_chord.chords[1] = CH(9);
    HarmonyPlagalMotion bad_placement = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    bad_placement.placement = HARMONY_PLAGAL_PLACEMENT_COUNT;
    HarmonyPlagalMotion bad_mode = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyPlagalMotion good = example_plagal_after_cadence(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_plagal_motion_valid(NULL));
    assert(!harmony_plagal_motion_valid(&bad_chord));
    assert(!harmony_plagal_motion_valid(&bad_placement));
    assert(!harmony_plagal_motion_valid(&bad_mode));
    assert(!harmony_plagal_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_plagal_check(&good, NULL, &soft));
    assert(!harmony_plagal_check(&good, &hard, NULL));
    assert(!holds(PLG(PROGRESSION), &bad_chord));
    assert(!holds(PLG(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_example_after_a_cadence_breaks_no_rule();
    test_the_phrase_beginning_examples_break_only_the_soft_rule();
    test_progression();
    test_each_chord_has_its_own_notes_all_three();
    test_upper_voices_move_by_step_or_common_tone_in_root_position();
    test_the_inverted_tonic_has_no_step_rule();
    test_no_parallels_into_the_inverted_tonic();
    test_soprano_do_after_a_cadence();
    test_avoid_at_beginning_always_holds_after_a_cadence();
    test_invalid_subject();
    printf("test_harmony_plagal_rules: ok\n");
    return 0;
}
