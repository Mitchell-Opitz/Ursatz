#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_connection_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"

#define CON(name) HARMONY_CONNECTION_##name
#define BIT(rule) (1u << (rule))

static HarmonyConnection make(HarmonyMode mode, HarmonyChord a, HarmonyChord b, HarmonyChord c, const int s[3][4]) {
    HarmonyConnection k;
    memset(&k, 0, sizeof(k));
    k.mode = mode;
    k.chords[0] = a;
    k.chords[1] = b;
    k.chords[2] = c;
    memcpy(k.steps, s, sizeof(k.steps));
    return k;
}

/* I vi IV over do la fa. */
static HarmonyConnection example_vi(HarmonyMode mode) {
    static const int s[3][4] = {{4, 2, 0, -7}, {5, 2, 0, -9}, {5, 3, 0, -11}};
    return make(mode, CH(1), CH(6), CH(4), s);
}

/* I IV6 IV over do la fa. */
static HarmonyConnection example_iv6(HarmonyMode mode) {
    static const int s[3][4] = {{4, 2, 0, -7}, {5, 3, 0, -9}, {5, 3, 0, -11}};
    return make(mode, CH(1), CHI(4, 1), CH(4), s);
}

/* I vi ii6 over do la fa. */
static HarmonyConnection example_ii6(HarmonyMode mode) {
    static const int s[3][4] = {{4, 2, 0, -7}, {5, 2, 0, -9}, {5, 3, 1, -11}};
    return make(mode, CH(1), CH(6), CHI(2, 1), s);
}

static bool holds(HarmonyConnectionRule rule, const HarmonyConnection *k) {
    return harmony_connection_rule_holds(rule, k);
}

static void test_progression_is(void) {
    HarmonyChord vi_iv[3] = {CH(1), CH(6), CH(4)};
    HarmonyChord iv6_ii6[3] = {CH(1), CHI(4, 1), CHI(2, 1)};
    HarmonyChord inverted_vi[3] = {CH(1), CHI(6, 1), CH(4)};
    HarmonyChord root_ii[3] = {CH(1), CH(6), CH(2)};
    HarmonyChord not_tonic[3] = {CHI(1, 1), CH(6), CH(4)};
    HarmonyChord bad[3] = {CH(1), CH(6), CH(9)};
    assert(harmony_connection_progression_is(vi_iv));
    assert(harmony_connection_progression_is(iv6_ii6));
    assert(!harmony_connection_progression_is(inverted_vi));
    assert(!harmony_connection_progression_is(root_ii));
    assert(!harmony_connection_progression_is(not_tonic));
    assert(!harmony_connection_progression_is(bad));
    assert(!harmony_connection_progression_is(NULL));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_connection_rule, harmony_connection_constraint,
                                    HARMONY_CONNECTION_RULE_COUNT);
    for (int i = 0; i < HARMONY_CONNECTION_RULE_COUNT; i++) {
        assert(constraint_type(harmony_connection_constraint((HarmonyConnectionRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_CONNECTION_RULE_COUNT] = {"progression", "bass-line", "chord-notes",
                                                                 "step-or-common-tone", "no-parallels"};
    for (int i = 0; i < HARMONY_CONNECTION_RULE_COUNT; i++) {
        assert(strcmp(harmony_connection_rule_name((HarmonyConnectionRule)i), expected[i]) == 0);
    }
    assert(harmony_connection_rule_name(CON(RULE_COUNT)) == NULL);
}

static void test_the_worked_examples_break_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode md = (HarmonyMode)mode;
        HarmonyConnection examples[] = {example_vi(md), example_iv6(md), example_ii6(md)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            for (int rule = 0; rule < HARMONY_CONNECTION_RULE_COUNT; rule++) {
                assert(holds((HarmonyConnectionRule)rule, &examples[i]));
            }
            unsigned hard = 99, soft = 99;
            assert(harmony_connection_check(&examples[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_progression(void) {
    HarmonyConnection k = example_vi(HARMONY_MODE_MAJOR);
    k.chords[1] = CH(2);
    assert(!holds(CON(PROGRESSION), &k));
}

static void test_the_bass_arpeggiates_down_do_la_fa(void) {
    HarmonyConnection up = example_vi(HARMONY_MODE_MAJOR);
    up.steps[1][VOICE_PART_BASS] = -2; /* la above the do */
    HarmonyConnection wrong_note = example_vi(HARMONY_MODE_MAJOR);
    wrong_note.steps[2][VOICE_PART_BASS] = -10; /* sol, not fa */
    assert(!holds(CON(BASS_LINE), &up));
    assert(!holds(CON(BASS_LINE), &wrong_note));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyConnection foreign = example_vi(HARMONY_MODE_MAJOR);
    foreign.steps[1][VOICE_PART_SOPRANO] = 6; /* ti is not in vi */
    assert(!holds(CON(CHORD_NOTES), &foreign));
}

static void test_the_upper_voices_move_by_step_or_common_tone(void) {
    HarmonyConnection k = example_vi(HARMONY_MODE_MAJOR);
    k.steps[2][VOICE_PART_ALTO] = 5; /* mi up to la: a leap */
    assert(!holds(CON(STEP_OR_COMMON_TONE), &k));
}

static void test_no_parallel_octaves(void) {
    HarmonyConnection k = example_vi(HARMONY_MODE_MAJOR);
    k.steps[0][VOICE_PART_TENOR] = -7; /* do over the bass do */
    k.steps[1][VOICE_PART_TENOR] = -9; /* la over the bass la, both down */
    assert(!holds(CON(NO_PARALLELS), &k));
}

static void test_invalid_subject(void) {
    HarmonyConnection bad_chord = example_vi(HARMONY_MODE_MAJOR);
    bad_chord.chords[1] = CH(9);
    HarmonyConnection bad_mode = example_vi(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyConnection good = example_vi(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_connection_valid(NULL));
    assert(!harmony_connection_valid(&bad_chord));
    assert(!harmony_connection_valid(&bad_mode));
    assert(!harmony_connection_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_connection_check(&good, NULL, &soft));
    assert(!harmony_connection_check(&good, &hard, NULL));
    assert(!holds(CON(PROGRESSION), &bad_chord));
    assert(!holds(CON(RULE_COUNT), &good));
}

int main(void) {
    test_progression_is();
    test_rules_reference_framework();
    test_names();
    test_the_worked_examples_break_no_rule();
    test_progression();
    test_the_bass_arpeggiates_down_do_la_fa();
    test_each_chord_has_its_own_notes();
    test_the_upper_voices_move_by_step_or_common_tone();
    test_no_parallel_octaves();
    test_invalid_subject();
    printf("test_harmony_connection_rules: ok\n");
    return 0;
}
