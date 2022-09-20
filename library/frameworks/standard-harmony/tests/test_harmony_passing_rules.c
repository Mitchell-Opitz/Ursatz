#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_passing_rules.h"
#include "harmony_rule_check.h"
#include "harmony_six_four_test_util.h"

#define PASS(name) HARMONY_PASSING_##name

static bool holds(HarmonyPassingRule rule, const HarmonySixFour *s) {
    return harmony_passing_rule_holds(rule, s);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_passing_rule, harmony_passing_constraint, HARMONY_PASSING_RULE_COUNT);
    for (int i = 0; i < HARMONY_PASSING_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_passing_constraint((HarmonyPassingRule)i));
        assert(type == (i <= PASS(SIX_FOUR_NOTES) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PASSING_RULE_COUNT] = {
        "is-six-four", "stepwise-bass", "same-function", "six-four-notes",
        "prolongs-tonic-or-predominant", "soprano-by-step", "inner-voices-move-little"};
    for (int i = 0; i < HARMONY_PASSING_RULE_COUNT; i++) {
        assert(strcmp(harmony_passing_rule_name((HarmonyPassingRule)i), expected[i]) == 0);
    }
    assert(harmony_passing_rule_name(PASS(RULE_COUNT)) == NULL);
}

static void test_the_worked_examples_break_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonySixFour examples[] = {example_passing_tonic(m), example_passing_backward(m),
                                     example_passing_predominant(m)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            for (int rule = 0; rule < HARMONY_PASSING_RULE_COUNT; rule++) {
                assert(holds((HarmonyPassingRule)rule, &examples[i]));
            }
            unsigned hard = 99, soft = 99;
            assert(harmony_passing_check(&examples[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_stepwise_bass(void) {
    /* The passing tone turns back, or the bass leaps. */
    HarmonySixFour turns = example_passing_tonic(HARMONY_MODE_MAJOR);
    turns.steps[2][VOICE_PART_BASS] = -7;
    HarmonySixFour leaps = example_passing_tonic(HARMONY_MODE_MAJOR);
    leaps.steps[0][VOICE_PART_BASS] = -9;
    HarmonySixFour static_bass = example_passing_tonic(HARMONY_MODE_MAJOR);
    static_bass.steps[2][VOICE_PART_BASS] = -6;
    assert(!holds(PASS(STEPWISE_BASS), &turns));
    assert(!holds(PASS(STEPWISE_BASS), &leaps));
    assert(!holds(PASS(STEPWISE_BASS), &static_bass));
}

static void test_both_sides_have_the_same_function(void) {
    HarmonySixFour mixed = example_passing_tonic(HARMONY_MODE_MAJOR);
    mixed.chords[2] = CHI(5, 1);
    assert(!holds(PASS(SAME_FUNCTION), &mixed));
    /* It is a hard rule. */
    unsigned hard = 0, soft = 0;
    assert(harmony_passing_check(&mixed, &hard, &soft));
    assert((hard & (1u << PASS(SAME_FUNCTION))) != 0);
    /* Two dominants are the same function, though it is not the usual place for one. */
    HarmonySixFour dominants = example_passing_tonic(HARMONY_MODE_MAJOR);
    dominants.chords[0] = CH(5);
    dominants.chords[2] = CHI(5, 1);
    assert(holds(PASS(SAME_FUNCTION), &dominants));
    assert(!holds(PASS(PROLONGS_TONIC_OR_PREDOMINANT), &dominants));
    unsigned h = 99, sft = 0;
    HarmonySixFour only_soft = dominants;
    assert(harmony_passing_check(&only_soft, &h, &sft));
    assert((sft & (1u << PASS(PROLONGS_TONIC_OR_PREDOMINANT))) != 0);
}

static void test_six_four_notes(void) {
    /* The bass has to be doubled, and the fourth and the sixth above it each sound once. */
    HarmonySixFour no_double = example_passing_tonic(HARMONY_MODE_MAJOR);
    no_double.steps[1][VOICE_PART_ALTO] = 0; /* do, so re is not doubled */
    HarmonySixFour no_sixth = example_passing_tonic(HARMONY_MODE_MAJOR);
    no_sixth.steps[1][VOICE_PART_SOPRANO] = -3; /* sol twice, no ti */
    HarmonySixFour three_roots = example_passing_tonic(HARMONY_MODE_MAJOR);
    three_roots.steps[1][VOICE_PART_SOPRANO] = 1; /* re, re, re? bass, alto, soprano */
    assert(!holds(PASS(SIX_FOUR_NOTES), &no_double));
    assert(!holds(PASS(SIX_FOUR_NOTES), &no_sixth));
    assert(!holds(PASS(SIX_FOUR_NOTES), &three_roots));
}

static void test_soprano_moves_by_step(void) {
    /* The static line is the one to avoid, here. */
    HarmonySixFour still = example_passing_tonic(HARMONY_MODE_MAJOR);
    still.steps[0][VOICE_PART_SOPRANO] = 6;
    still.steps[2][VOICE_PART_SOPRANO] = 6;
    HarmonySixFour leaps = example_passing_tonic(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_SOPRANO] = 8;
    assert(!holds(PASS(SOPRANO_BY_STEP), &still));
    assert(!holds(PASS(SOPRANO_BY_STEP), &leaps));
    /* It is advice: nothing hard breaks. */
    unsigned hard = 99, soft = 0;
    assert(harmony_passing_check(&still, &hard, &soft));
    assert(hard == 0 && (soft & (1u << PASS(SOPRANO_BY_STEP))) != 0);
}

static void test_inner_voices_move_little(void) {
    HarmonySixFour leaps = example_passing_tonic(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_TENOR] = -6; /* the tenor leaps up a third */
    assert(!holds(PASS(INNER_VOICES_MOVE_LITTLE), &leaps));
    HarmonySixFour alto_leaps = example_passing_tonic(HARMONY_MODE_MAJOR);
    alto_leaps.steps[1][VOICE_PART_ALTO] = 0;
    alto_leaps.steps[2][VOICE_PART_ALTO] = 2;
    assert(!holds(PASS(INNER_VOICES_MOVE_LITTLE), &alto_leaps));
    /* The soprano and the bass are not inner voices. */
    HarmonySixFour soprano = example_passing_tonic(HARMONY_MODE_MAJOR);
    soprano.steps[2][VOICE_PART_SOPRANO] = 9;
    assert(holds(PASS(INNER_VOICES_MOVE_LITTLE), &soprano));
}

static void test_not_a_six_four(void) {
    HarmonySixFour s = example_passing_tonic(HARMONY_MODE_MAJOR);
    s.chords[1] = CH(5);
    assert(!holds(PASS(IS_SIX_FOUR), &s));
    unsigned hard = 0, soft = 0;
    assert(harmony_passing_check(&s, &hard, &soft));
    assert((hard & (1u << PASS(IS_SIX_FOUR))) != 0);
}

static void test_invalid_subject(void) {
    HarmonySixFour bad_mode = example_passing_tonic(HARMONY_MODE_COUNT);
    HarmonySixFour bad_chord = example_passing_tonic(HARMONY_MODE_MAJOR);
    bad_chord.chords[1] = CH(9);
    HarmonySixFour *bad[] = {&bad_mode, &bad_chord};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        for (int rule = 0; rule < HARMONY_PASSING_RULE_COUNT; rule++) {
            assert(!holds((HarmonyPassingRule)rule, bad[i]));
        }
        assert(!harmony_passing_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    HarmonySixFour ok = example_passing_tonic(HARMONY_MODE_MAJOR);
    assert(!holds(PASS(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_examples_break_no_rule();
    test_stepwise_bass();
    test_both_sides_have_the_same_function();
    test_six_four_notes();
    test_soprano_moves_by_step();
    test_inner_voices_move_little();
    test_not_a_six_four();
    test_invalid_subject();
    printf("test_harmony_passing_rules: ok\n");
    return 0;
}
