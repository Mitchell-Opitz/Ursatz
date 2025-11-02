#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_deceptive_rules.h"
#include "harmony_deceptive_test_util.h"
#include "harmony_rule_check.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "deceptive", HARMONY_DECEPTIVE_RULE_COUNT);
#define harmony_deceptive_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_deceptive_constraint, harmony_deceptive_preference, harmony_deceptive_rule_holds, HarmonyDeceptiveMotion, which, subject)

#define DEC(name) HARMONY_DECEPTIVE_##name
#define BIT(rule) (1u << (rule))

static bool holds(HarmonyDeceptiveRule rule, const HarmonyDeceptiveMotion *m) {
    return harmony_deceptive_rule_holds(rule, m);
}

static void expect_breaks(const HarmonyDeceptiveMotion *m, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_deceptive_check(m, &h, &s));
    assert(h == hard && s == soft);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_deceptive_rule, harmony_deceptive_constraint, HARMONY_DECEPTIVE_RULE_COUNT);
    for (int i = 0; i < HARMONY_DECEPTIVE_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_deceptive_constraint((HarmonyDeceptiveRule)i));
        assert(type == (i <= DEC(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_DECEPTIVE_RULE_COUNT] = {
        "progression", "chord-notes", "resolves", "contrary-motion", "no-parallels", "prefer-vi"};
    for (int i = 0; i < HARMONY_DECEPTIVE_RULE_COUNT; i++) {
        assert(strcmp(harmony_deceptive_rule_name((HarmonyDeceptiveRule)i), expected[i]) == 0);
    }
    assert(harmony_deceptive_rule_name(DEC(RULE_COUNT)) == NULL);
}

static void test_the_worked_examples(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode md = (HarmonyMode)mode;
        HarmonyDeceptiveMotion vi = example_deceptive_vi(md);
        HarmonyDeceptiveMotion v7 = example_deceptive_v7_vi(md);
        HarmonyDeceptiveMotion iv6 = example_deceptive_iv6(md);
        for (int rule = 0; rule < HARMONY_DECEPTIVE_RULE_COUNT; rule++) {
            assert(holds((HarmonyDeceptiveRule)rule, &vi));
            assert(holds((HarmonyDeceptiveRule)rule, &v7));
        }
        expect_breaks(&vi, 0, 0);
        expect_breaks(&v7, 0, 0);
        expect_breaks(&iv6, 0, BIT(DEC(PREFER_VI)));
        assert(!holds(DEC(PREFER_VI), &iv6));
    }
}

static void test_progression(void) {
    HarmonyDeceptiveMotion to_tonic = example_deceptive_vi(HARMONY_MODE_MAJOR);
    to_tonic.chords[1] = CH(1);
    assert(!holds(DEC(PROGRESSION), &to_tonic));
    assert(!holds(DEC(CHORD_NOTES), &to_tonic));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyDeceptiveMotion no_third = example_deceptive_vi(HARMONY_MODE_MAJOR);
    no_third.steps[0][VOICE_PART_SOPRANO] = 4; /* sol sol re sol: no ti */
    HarmonyDeceptiveMotion foreign = example_deceptive_vi(HARMONY_MODE_MAJOR);
    foreign.steps[1][VOICE_PART_ALTO] = 3; /* fa is not in vi */
    HarmonyDeceptiveMotion wrong_bass = example_deceptive_vi(HARMONY_MODE_MAJOR);
    wrong_bass.steps[1][VOICE_PART_BASS] = -4; /* mi in the bass of vi */
    HarmonyDeceptiveMotion v7_doubled = example_deceptive_v7_vi(HARMONY_MODE_MAJOR);
    v7_doubled.steps[0][VOICE_PART_ALTO] = 1; /* re twice, no fa */
    assert(!holds(DEC(CHORD_NOTES), &no_third));
    assert(!holds(DEC(CHORD_NOTES), &foreign));
    assert(!holds(DEC(CHORD_NOTES), &wrong_bass));
    assert(!holds(DEC(CHORD_NOTES), &v7_doubled));
}

static void test_ti_goes_up_to_do(void) {
    HarmonyDeceptiveMotion m = example_deceptive_vi(HARMONY_MODE_MAJOR);
    m.steps[1][VOICE_PART_SOPRANO] = 5; /* ti falls to la */
    assert(!holds(DEC(RESOLVES), &m));
    expect_breaks(&m, BIT(DEC(RESOLVES)), 0);
}

static void test_the_seventh_goes_down_to_mi(void) {
    HarmonyDeceptiveMotion m = example_deceptive_v7_vi(HARMONY_MODE_MAJOR);
    m.steps[1][VOICE_PART_ALTO] = 4; /* fa goes up */
    assert(!holds(DEC(RESOLVES), &m));
}

static void test_upper_voices_move_down_against_the_bass_in_vi(void) {
    HarmonyDeceptiveMotion m = example_deceptive_vi(HARMONY_MODE_MAJOR);
    m.steps[1][VOICE_PART_TENOR] = 2; /* re goes up to mi */
    assert(!holds(DEC(CONTRARY_MOTION), &m));
    assert(!holds(DEC(NO_PARALLELS), &m)); /* the tenor and bass make parallel fifths */
    expect_breaks(&m, BIT(DEC(CONTRARY_MOTION)) | BIT(DEC(NO_PARALLELS)), 0);
}

static void test_iv6_has_no_contrary_motion_rule(void) {
    HarmonyDeceptiveMotion m = example_deceptive_iv6(HARMONY_MODE_MAJOR);
    m.steps[1][VOICE_PART_TENOR] = 2;
    assert(holds(DEC(CONTRARY_MOTION), &m));
}

static void test_invalid_subject(void) {
    HarmonyDeceptiveMotion bad_chord = example_deceptive_vi(HARMONY_MODE_MAJOR);
    bad_chord.chords[1] = CH(9);
    HarmonyDeceptiveMotion bad_mode = example_deceptive_vi(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyDeceptiveMotion good = example_deceptive_vi(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_deceptive_motion_valid(NULL));
    assert(!harmony_deceptive_motion_valid(&bad_chord));
    assert(!harmony_deceptive_motion_valid(&bad_mode));
    assert(!harmony_deceptive_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_deceptive_check(&good, NULL, &soft));
    assert(!harmony_deceptive_check(&good, &hard, NULL));
    assert(!holds(DEC(PROGRESSION), &bad_chord));
    assert(!holds(DEC(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_examples();
    test_progression();
    test_each_chord_has_its_own_notes();
    test_ti_goes_up_to_do();
    test_the_seventh_goes_down_to_mi();
    test_upper_voices_move_down_against_the_bass_in_vi();
    test_iv6_has_no_contrary_motion_rule();
    test_invalid_subject();
    printf("test_harmony_deceptive_rules: ok\n");
    return 0;
}
