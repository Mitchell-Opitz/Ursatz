#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_phrygian_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "phrygian", HARMONY_PHRYGIAN_RULE_COUNT);
#define harmony_phrygian_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_phrygian_constraint, harmony_phrygian_preference, harmony_phrygian_rule_holds, HarmonyPhrygian, which, subject)

#define PHC(name) HARMONY_PHRYGIAN_##name
#define BIT(rule) (1u << (rule))

/* iv6 V over le sol in minor: do doubled in the iv6, the soprano fa sol. */
static HarmonyPhrygian example(HarmonyMode mode) {
    static const int s[2][4] = {{3, 0, -7, -9}, {4, -1, -6, -10}};
    HarmonyPhrygian p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.chords[0] = CHI(4, 1);
    p.chords[1] = CH(5);
    memcpy(p.steps, s, sizeof(p.steps));
    return p;
}

static bool holds(HarmonyPhrygianRule rule, const HarmonyPhrygian *p) {
    return harmony_phrygian_rule_holds(rule, p);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_phrygian_rule, harmony_phrygian_constraint, HARMONY_PHRYGIAN_RULE_COUNT);
    for (int i = 0; i < HARMONY_PHRYGIAN_RULE_COUNT; i++) {
        assert(constraint_type(harmony_phrygian_constraint((HarmonyPhrygianRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PHRYGIAN_RULE_COUNT] = {"progression", "chord-notes", "doubled-do",
                                                               "no-augmented-second", "no-parallels"};
    for (int i = 0; i < HARMONY_PHRYGIAN_RULE_COUNT; i++) {
        assert(strcmp(harmony_phrygian_rule_name((HarmonyPhrygianRule)i), expected[i]) == 0);
    }
    assert(harmony_phrygian_rule_name(PHC(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_breaks_no_rule(void) {
    HarmonyPhrygian p = example(HARMONY_MODE_MINOR);
    for (int rule = 0; rule < HARMONY_PHRYGIAN_RULE_COUNT; rule++) {
        assert(holds((HarmonyPhrygianRule)rule, &p));
    }
    unsigned hard = 99, soft = 99;
    assert(harmony_phrygian_check(&p, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_it_needs_a_minor_key(void) {
    HarmonyPhrygian p = example(HARMONY_MODE_MAJOR);
    assert(!holds(PHC(PROGRESSION), &p));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyPhrygian foreign = example(HARMONY_MODE_MINOR);
    foreign.steps[1][VOICE_PART_ALTO] = 0; /* do is not in V */
    HarmonyPhrygian wrong_bass = example(HARMONY_MODE_MINOR);
    wrong_bass.steps[0][VOICE_PART_BASS] = -10; /* sol in the bass of iv6 */
    assert(!holds(PHC(CHORD_NOTES), &foreign));
    assert(!holds(PHC(CHORD_NOTES), &wrong_bass));
}

static void test_do_is_doubled_in_the_iv6(void) {
    HarmonyPhrygian p = example(HARMONY_MODE_MINOR);
    p.steps[0][VOICE_PART_TENOR] = -4; /* fa, so do is in only one voice */
    assert(holds(PHC(CHORD_NOTES), &p));
    assert(!holds(PHC(DOUBLED_DO), &p));
}

static void test_no_augmented_second_from_le_to_ti(void) {
    HarmonyPhrygian p = example(HARMONY_MODE_MINOR);
    p.steps[0][VOICE_PART_ALTO] = -2; /* le */
    p.steps[1][VOICE_PART_ALTO] = -1; /* up to ti */
    assert(!holds(PHC(NO_AUGMENTED_SECOND), &p));
}

static void test_no_parallel_octaves(void) {
    HarmonyPhrygian p = example(HARMONY_MODE_MINOR);
    p.steps[0][VOICE_PART_TENOR] = -2; /* le over the bass le */
    p.steps[1][VOICE_PART_TENOR] = -3; /* sol over the bass sol, both down */
    assert(!holds(PHC(NO_PARALLELS), &p));
}

static void test_invalid_subject(void) {
    HarmonyPhrygian bad_chord = example(HARMONY_MODE_MINOR);
    bad_chord.chords[1] = CH(9);
    HarmonyPhrygian bad_mode = example(HARMONY_MODE_MINOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyPhrygian good = example(HARMONY_MODE_MINOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_phrygian_valid(NULL));
    assert(!harmony_phrygian_valid(&bad_chord));
    assert(!harmony_phrygian_valid(&bad_mode));
    assert(!harmony_phrygian_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_phrygian_check(&good, NULL, &soft));
    assert(!harmony_phrygian_check(&good, &hard, NULL));
    assert(!holds(PHC(PROGRESSION), &bad_chord));
    assert(!holds(PHC(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_example_breaks_no_rule();
    test_it_needs_a_minor_key();
    test_each_chord_has_its_own_notes();
    test_do_is_doubled_in_the_iv6();
    test_no_augmented_second_from_le_to_ti();
    test_no_parallel_octaves();
    test_invalid_subject();
    printf("test_harmony_phrygian_rules: ok\n");
    return 0;
}
