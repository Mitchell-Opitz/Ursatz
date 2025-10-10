#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_iv6_prolongation_rules.h"
#include "harmony_iv6_prolongation_test_util.h"
#include "harmony_rule_check.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "iv6-prolongation", HARMONY_IV6_PROLONGATION_RULE_COUNT);
#define harmony_iv6_prolongation_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_iv6_prolongation_constraint, harmony_iv6_prolongation_rule_holds, HarmonyIv6Prolongation, which, subject)

#define IV6(name) HARMONY_IV6_PROLONGATION_##name
#define BIT(rule) (1u << (rule))

static bool holds(HarmonyIv6ProlongationRule rule, const HarmonyIv6Prolongation *p) {
    return harmony_iv6_prolongation_rule_holds(rule, p);
}

static void expect_breaks(const HarmonyIv6Prolongation *p, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_iv6_prolongation_check(p, &h, &s));
    assert(h == hard && s == soft);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_iv6_prolongation_rule, harmony_iv6_prolongation_constraint,
                                    HARMONY_IV6_PROLONGATION_RULE_COUNT);
    for (int i = 0; i < HARMONY_IV6_PROLONGATION_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_iv6_prolongation_constraint((HarmonyIv6ProlongationRule)i));
        assert(type == (i <= IV6(NO_DOUBLED_LEADING_TONE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_IV6_PROLONGATION_RULE_COUNT] = {
        "progression", "bass-line",    "chord-notes", "no-parallels",
        "no-doubled-leading-tone", "soprano-line", "prefer-v6",  "avoid-in-minor"};
    for (int i = 0; i < HARMONY_IV6_PROLONGATION_RULE_COUNT; i++) {
        assert(strcmp(harmony_iv6_prolongation_rule_name((HarmonyIv6ProlongationRule)i), expected[i]) == 0);
    }
    assert(harmony_iv6_prolongation_rule_name(IV6(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_in_major_breaks_no_rule(void) {
    HarmonyIv6Prolongation p = example_iv6_v6(HARMONY_MODE_MAJOR);
    for (int rule = 0; rule < HARMONY_IV6_PROLONGATION_RULE_COUNT; rule++) {
        assert(holds((HarmonyIv6ProlongationRule)rule, &p));
    }
    expect_breaks(&p, 0, 0);
}

static void test_the_root_position_v_breaks_only_the_soft_rule(void) {
    HarmonyIv6Prolongation p = example_iv6_v(HARMONY_MODE_MAJOR);
    expect_breaks(&p, 0, BIT(IV6(PREFER_V6)));
}

static void test_minor_breaks_only_the_soft_rule(void) {
    HarmonyIv6Prolongation p = example_iv6_v6(HARMONY_MODE_MINOR);
    expect_breaks(&p, 0, BIT(IV6(AVOID_IN_MINOR)));
}

static void test_progression_and_bass_line(void) {
    HarmonyIv6Prolongation not_iv6 = example_iv6_v6(HARMONY_MODE_MAJOR);
    not_iv6.chords[1] = CH(4);
    HarmonyIv6Prolongation wrong_bass = example_iv6_v6(HARMONY_MODE_MAJOR);
    wrong_bass.steps[2][VOICE_PART_BASS] = -10; /* sol in the bass of V6 */
    assert(!holds(IV6(PROGRESSION), &not_iv6));
    assert(!holds(IV6(BASS_LINE), &not_iv6));
    assert(holds(IV6(PROGRESSION), &wrong_bass));
    assert(!holds(IV6(BASS_LINE), &wrong_bass));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyIv6Prolongation foreign = example_iv6_v6(HARMONY_MODE_MAJOR);
    foreign.steps[1][VOICE_PART_ALTO] = 4; /* sol is not in IV6 */
    HarmonyIv6Prolongation no_third = example_iv6_v6(HARMONY_MODE_MAJOR);
    no_third.steps[1][VOICE_PART_TENOR] = 7; /* do do do... no fa */
    no_third.steps[1][VOICE_PART_ALTO] = 0;
    assert(!holds(IV6(CHORD_NOTES), &foreign));
    assert(!holds(IV6(CHORD_NOTES), &no_third));
}

static void test_no_parallel_octaves(void) {
    HarmonyIv6Prolongation p = example_iv6_v6(HARMONY_MODE_MAJOR);
    p.steps[0][VOICE_PART_ALTO] = 0; /* do over the bass do: an octave */
    p.steps[1][VOICE_PART_ALTO] = -2; /* la over the bass la: the octave again, both down */
    assert(!holds(IV6(NO_PARALLELS), &p));
}

static void test_the_leading_tone_is_not_doubled(void) {
    HarmonyIv6Prolongation p = example_iv6_v(HARMONY_MODE_MAJOR);
    p.steps[2][VOICE_PART_TENOR] = 6; /* ti in the alto and the tenor */
    assert(!holds(IV6(NO_DOUBLED_LEADING_TONE), &p));
    HarmonyIv6Prolongation once = example_iv6_v6(HARMONY_MODE_MAJOR);
    assert(holds(IV6(NO_DOUBLED_LEADING_TONE), &once));
}

static void test_the_soprano_line(void) {
    HarmonyIv6Prolongation p = example_iv6_v6(HARMONY_MODE_MAJOR);
    p.steps[2][VOICE_PART_SOPRANO] = 4; /* sol, not re */
    assert(!holds(IV6(SOPRANO_LINE), &p));
}

static void test_invalid_subject(void) {
    HarmonyIv6Prolongation bad_chord = example_iv6_v6(HARMONY_MODE_MAJOR);
    bad_chord.chords[3] = CH(9);
    HarmonyIv6Prolongation bad_mode = example_iv6_v6(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyIv6Prolongation good = example_iv6_v6(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_iv6_prolongation_valid(NULL));
    assert(!harmony_iv6_prolongation_valid(&bad_chord));
    assert(!harmony_iv6_prolongation_valid(&bad_mode));
    assert(!harmony_iv6_prolongation_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_iv6_prolongation_check(&good, NULL, &soft));
    assert(!harmony_iv6_prolongation_check(&good, &hard, NULL));
    assert(!holds(IV6(PROGRESSION), &bad_chord));
    assert(!holds(IV6(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_example_in_major_breaks_no_rule();
    test_the_root_position_v_breaks_only_the_soft_rule();
    test_minor_breaks_only_the_soft_rule();
    test_progression_and_bass_line();
    test_each_chord_has_its_own_notes();
    test_no_parallel_octaves();
    test_the_leading_tone_is_not_doubled();
    test_the_soprano_line();
    test_invalid_subject();
    printf("test_harmony_iv6_prolongation_rules: ok\n");
    return 0;
}
