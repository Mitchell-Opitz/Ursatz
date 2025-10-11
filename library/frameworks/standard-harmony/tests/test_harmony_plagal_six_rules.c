#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_plagal_six_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "plagal-six", HARMONY_PLAGAL_SIX_RULE_COUNT);
#define harmony_plagal_six_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_plagal_six_constraint, harmony_plagal_six_rule_holds, HarmonyPlagalSix, which, subject)

#define PS(name) HARMONY_PLAGAL_SIX_##name
#define BIT(rule) (1u << (rule))

/* I (IV6) I6 over do la mi, the bass leaping down each time, the soprano mi fa sol. */
static HarmonyPlagalSix example(HarmonyMode mode) {
    static const int s0[3][4] = {{2, 0, -3, -7}, {3, 0, -2, -9}, {4, 0, -3, -12}};
    HarmonyPlagalSix p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.chords[0] = CH(1);
    p.chords[1] = CHI(4, 1);
    p.chords[2] = CHI(1, 1);
    memcpy(p.steps, s0, sizeof(p.steps));
    return p;
}

static bool holds(HarmonyPlagalSixRule rule, const HarmonyPlagalSix *p) {
    return harmony_plagal_six_rule_holds(rule, p);
}

static void expect_breaks(const HarmonyPlagalSix *p, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_plagal_six_check(p, &h, &s));
    assert(h == hard && s == soft);
}

static void test_progression_is(void) {
    HarmonyChord good[3] = {CH(1), CHI(4, 1), CHI(1, 1)};
    HarmonyChord root_iv[3] = {CH(1), CH(4), CHI(1, 1)};
    HarmonyChord root_end[3] = {CH(1), CHI(4, 1), CH(1)};
    HarmonyChord seventh[3] = {CH(1), CHI(4, 1), CH7I(1, 1)};
    HarmonyChord bad[3] = {CH(1), CHI(4, 1), CH(9)};
    assert(harmony_plagal_six_progression_is(good));
    assert(!harmony_plagal_six_progression_is(root_iv));
    assert(!harmony_plagal_six_progression_is(root_end));
    assert(!harmony_plagal_six_progression_is(seventh));
    assert(!harmony_plagal_six_progression_is(bad));
    assert(!harmony_plagal_six_progression_is(NULL));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_plagal_six_rule, harmony_plagal_six_constraint,
                                    HARMONY_PLAGAL_SIX_RULE_COUNT);
    for (int i = 0; i < HARMONY_PLAGAL_SIX_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_plagal_six_constraint((HarmonyPlagalSixRule)i));
        assert(type == (i <= PS(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PLAGAL_SIX_RULE_COUNT] = {
        "progression", "bass-line", "chord-notes", "step-or-common-tone", "no-parallels", "soprano-mi-fa-sol"};
    for (int i = 0; i < HARMONY_PLAGAL_SIX_RULE_COUNT; i++) {
        assert(strcmp(harmony_plagal_six_rule_name((HarmonyPlagalSixRule)i), expected[i]) == 0);
    }
    assert(harmony_plagal_six_rule_name(PS(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_breaks_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyPlagalSix p = example((HarmonyMode)mode);
        for (int rule = 0; rule < HARMONY_PLAGAL_SIX_RULE_COUNT; rule++) {
            assert(holds((HarmonyPlagalSixRule)rule, &p));
        }
        expect_breaks(&p, 0, 0);
    }
}

static void test_progression(void) {
    HarmonyPlagalSix p = example(HARMONY_MODE_MAJOR);
    p.chords[2] = CH(1);
    assert(!holds(PS(PROGRESSION), &p));
}

static void test_the_bass_arpeggiates_down(void) {
    HarmonyPlagalSix up = example(HARMONY_MODE_MAJOR);
    up.steps[2][VOICE_PART_BASS] = -5; /* mi, but above the la */
    HarmonyPlagalSix wrong_note = example(HARMONY_MODE_MAJOR);
    wrong_note.steps[1][VOICE_PART_BASS] = -10; /* sol, not la */
    assert(!holds(PS(BASS_LINE), &up));
    assert(!holds(PS(BASS_LINE), &wrong_note));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyPlagalSix foreign = example(HARMONY_MODE_MAJOR);
    foreign.steps[1][VOICE_PART_ALTO] = 1; /* re is not in IV6 */
    assert(!holds(PS(CHORD_NOTES), &foreign));
}

static void test_the_other_voices_move_by_step_or_common_tone(void) {
    HarmonyPlagalSix p = example(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_ALTO] = 2; /* do up to mi: a leap */
    assert(!holds(PS(STEP_OR_COMMON_TONE), &p));
}

static void test_no_parallel_octaves(void) {
    HarmonyPlagalSix p = example(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_ALTO] = -2; /* la over the bass la */
    p.steps[2][VOICE_PART_ALTO] = -5; /* mi over the bass mi, both down */
    assert(!holds(PS(NO_PARALLELS), &p));
}

static void test_the_soprano_is_mi_fa_sol(void) {
    HarmonyPlagalSix p = example(HARMONY_MODE_MAJOR);
    p.steps[2][VOICE_PART_SOPRANO] = 7; /* do, not sol */
    assert(!holds(PS(SOPRANO_MI_FA_SOL), &p));
}

static void test_invalid_subject(void) {
    HarmonyPlagalSix bad_chord = example(HARMONY_MODE_MAJOR);
    bad_chord.chords[0] = CH(9);
    HarmonyPlagalSix bad_mode = example(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyPlagalSix good = example(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_plagal_six_valid(NULL));
    assert(!harmony_plagal_six_valid(&bad_chord));
    assert(!harmony_plagal_six_valid(&bad_mode));
    assert(!harmony_plagal_six_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_plagal_six_check(&good, NULL, &soft));
    assert(!harmony_plagal_six_check(&good, &hard, NULL));
    assert(!holds(PS(PROGRESSION), &bad_chord));
    assert(!holds(PS(RULE_COUNT), &good));
}

int main(void) {
    test_progression_is();
    test_rules_reference_framework();
    test_names();
    test_the_worked_example_breaks_no_rule();
    test_progression();
    test_the_bass_arpeggiates_down();
    test_each_chord_has_its_own_notes();
    test_the_other_voices_move_by_step_or_common_tone();
    test_no_parallel_octaves();
    test_the_soprano_is_mi_fa_sol();
    test_invalid_subject();
    printf("test_harmony_plagal_six_rules: ok\n");
    return 0;
}
