#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_predominant_seventh_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"

#define PS(name) HARMONY_PREDOMINANT_SEVENTH_##name
#define BIT(rule) (1u << (rule))

static HarmonyPredominantSeventh make(HarmonyMode mode, HarmonyChord a, HarmonyChord b, HarmonyChord c,
                                      const int s[3][4]) {
    HarmonyPredominantSeventh p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.chords[0] = a;
    p.chords[1] = b;
    p.chords[2] = c;
    memcpy(p.steps, s, sizeof(p.steps));
    return p;
}

/* I6 ii7 V7 over mi re sol: the seventh (do, in the tenor) is a common tone from the I6 and
   falls to ti in the V7. */
static HarmonyPredominantSeventh example_ii7(HarmonyMode mode) {
    static const int s[3][4] = {{4, 2, 0, -5}, {5, 3, 0, -6}, {4, 3, -1, -3}};
    return make(mode, CHI(1, 1), CH7(2), CH7(5), s);
}

static bool holds(HarmonyPredominantSeventhRule rule, const HarmonyPredominantSeventh *p) {
    return harmony_predominant_seventh_rule_holds(rule, p);
}

static void expect_breaks(const HarmonyPredominantSeventh *p, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_predominant_seventh_check(p, &h, &s));
    assert(h == hard && s == soft);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_predominant_seventh_rule, harmony_predominant_seventh_constraint,
                                    HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT);
    for (int i = 0; i < HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_predominant_seventh_constraint((HarmonyPredominantSeventhRule)i));
        assert(type == (i <= PS(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT] = {
        "progression",  "chord-notes",    "seventh-approach", "seventh-resolves",
        "no-parallels", "preceded-by-i6", "root-position",    "vi7-connects"};
    for (int i = 0; i < HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT; i++) {
        assert(strcmp(harmony_predominant_seventh_rule_name((HarmonyPredominantSeventhRule)i), expected[i]) == 0);
    }
    assert(harmony_predominant_seventh_rule_name(PS(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_breaks_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyPredominantSeventh p = example_ii7((HarmonyMode)mode);
        for (int rule = 0; rule < HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT; rule++) {
            assert(holds((HarmonyPredominantSeventhRule)rule, &p));
        }
        expect_breaks(&p, 0, 0);
    }
}

static void test_progression(void) {
    HarmonyPredominantSeventh triad = example_ii7(HARMONY_MODE_MAJOR);
    triad.chords[1] = CH(2);
    HarmonyPredominantSeventh dominant = example_ii7(HARMONY_MODE_MAJOR);
    dominant.chords[1] = CH7(5);
    assert(!holds(PS(PROGRESSION), &triad));
    assert(!holds(PS(PROGRESSION), &dominant));
}

static void test_the_chord_has_its_own_notes(void) {
    HarmonyPredominantSeventh foreign = example_ii7(HARMONY_MODE_MAJOR);
    foreign.steps[1][VOICE_PART_ALTO] = 4; /* sol is not in ii7 */
    HarmonyPredominantSeventh no_seventh = example_ii7(HARMONY_MODE_MAJOR);
    no_seventh.steps[1][VOICE_PART_TENOR] = 1; /* re re fa la: no do */
    HarmonyPredominantSeventh wrong_bass = example_ii7(HARMONY_MODE_MAJOR);
    wrong_bass.steps[1][VOICE_PART_BASS] = -4; /* fa in the bass of a root position ii7 */
    assert(!holds(PS(CHORD_NOTES), &foreign));
    assert(!holds(PS(CHORD_NOTES), &no_seventh));
    assert(!holds(PS(CHORD_NOTES), &wrong_bass));
}

static void test_each_inversion_has_its_own_bass(void) {
    HarmonyPredominantSeventh p = example_ii7(HARMONY_MODE_MAJOR);
    p.chords[1] = CH7I(2, 1);
    p.steps[1][VOICE_PART_ALTO] = 1;
    p.steps[1][VOICE_PART_BASS] = -4; /* fa, the third, in the bass of ii6/5 */
    assert(holds(PS(CHORD_NOTES), &p));
    p.steps[1][VOICE_PART_BASS] = -6; /* re, the root */
    assert(!holds(PS(CHORD_NOTES), &p));
}

static void test_the_seventh_is_approached_by_step_or_common_tone(void) {
    HarmonyPredominantSeventh p = example_ii7(HARMONY_MODE_MAJOR);
    p.steps[0][VOICE_PART_TENOR] = -3; /* sol up to do: a leap */
    assert(!holds(PS(SEVENTH_APPROACH), &p));
}

static void test_the_seventh_resolves_down_by_step(void) {
    HarmonyPredominantSeventh stays = example_ii7(HARMONY_MODE_MAJOR);
    stays.steps[2][VOICE_PART_TENOR] = 0;
    HarmonyPredominantSeventh leaps = example_ii7(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_TENOR] = -3;
    assert(!holds(PS(SEVENTH_RESOLVES), &stays));
    assert(!holds(PS(SEVENTH_RESOLVES), &leaps));
}

static void test_no_parallel_fifths(void) {
    HarmonyPredominantSeventh p = example_ii7(HARMONY_MODE_MAJOR);
    p.steps[2][VOICE_PART_SOPRANO] = 8; /* re over the bass sol, both up from a fifth */
    assert(!holds(PS(NO_PARALLELS), &p));
}

static void test_ii7_is_best_preceded_by_i6(void) {
    HarmonyPredominantSeventh p = example_ii7(HARMONY_MODE_MAJOR);
    p.chords[0] = CH(1);
    assert(!holds(PS(PRECEDED_BY_I6), &p));
    expect_breaks(&p, 0, BIT(PS(PRECEDED_BY_I6)));
    p.chords[1] = CH7(4);
    assert(holds(PS(PRECEDED_BY_I6), &p));
}

static void test_iv7_and_vi7_are_in_root_position(void) {
    HarmonyPredominantSeventh p = example_ii7(HARMONY_MODE_MAJOR);
    p.chords[1] = CH7(4);
    assert(holds(PS(ROOT_POSITION), &p));
    p.chords[1] = CH7I(4, 1);
    assert(!holds(PS(ROOT_POSITION), &p));
    p.chords[1] = CH7I(6, 2);
    assert(!holds(PS(ROOT_POSITION), &p));
    p.chords[1] = CH7I(2, 1); /* ii6/5 is common in inversion */
    assert(holds(PS(ROOT_POSITION), &p));
    p.chords[1] = CH7I(3, 1);
    assert(holds(PS(ROOT_POSITION), &p));
}

static void test_vi7_connects_tonic_to_a_strong_predominant(void) {
    HarmonyPredominantSeventh p = example_ii7(HARMONY_MODE_MAJOR);
    p.chords[0] = CH(1);
    p.chords[1] = CH7(6);
    p.chords[2] = CH(4);
    assert(holds(PS(VI7_CONNECTS), &p));
    p.chords[2] = CHI(2, 1);
    assert(holds(PS(VI7_CONNECTS), &p));
    p.chords[0] = CH7(5); /* V7 to vi7 is not common */
    assert(!holds(PS(VI7_CONNECTS), &p));
    p.chords[0] = CH(1);
    p.chords[2] = CH(5); /* not on to a strong predominant */
    assert(!holds(PS(VI7_CONNECTS), &p));
    p.chords[1] = CH7(4); /* the rule is about vi7 only */
    assert(holds(PS(VI7_CONNECTS), &p));
}

static void test_invalid_subject(void) {
    HarmonyPredominantSeventh bad_chord = example_ii7(HARMONY_MODE_MAJOR);
    bad_chord.chords[2] = CH(9);
    HarmonyPredominantSeventh bad_mode = example_ii7(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyPredominantSeventh good = example_ii7(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_predominant_seventh_valid(NULL));
    assert(!harmony_predominant_seventh_valid(&bad_chord));
    assert(!harmony_predominant_seventh_valid(&bad_mode));
    assert(!harmony_predominant_seventh_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_predominant_seventh_check(&good, NULL, &soft));
    assert(!harmony_predominant_seventh_check(&good, &hard, NULL));
    assert(!holds(PS(PROGRESSION), &bad_chord));
    assert(!holds(PS(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_example_breaks_no_rule();
    test_progression();
    test_the_chord_has_its_own_notes();
    test_each_inversion_has_its_own_bass();
    test_the_seventh_is_approached_by_step_or_common_tone();
    test_the_seventh_resolves_down_by_step();
    test_no_parallel_fifths();
    test_ii7_is_best_preceded_by_i6();
    test_iv7_and_vi7_are_in_root_position();
    test_vi7_connects_tonic_to_a_strong_predominant();
    test_invalid_subject();
    printf("test_harmony_predominant_seventh_rules: ok\n");
    return 0;
}
