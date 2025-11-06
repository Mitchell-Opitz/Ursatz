#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_ii7_v7_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "ii7-v7", HARMONY_II7_V7_RULE_COUNT);
#define harmony_ii7_v7_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_ii7_v7_constraint, harmony_ii7_v7_preference, harmony_ii7_v7_rule_holds, HarmonyIi7V7, which, subject)

#define IV(name) HARMONY_II7_V7_##name

static HarmonyIi7V7 make(HarmonyMode mode, const int s[2][4]) {
    HarmonyIi7V7 p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.chords[0] = CH7(2);
    p.chords[1] = CH7(5);
    memcpy(p.steps, s, sizeof(p.steps));
    return p;
}

/* ii7 incomplete (re twice, no la) to a complete V7 over re sol. */
static HarmonyIi7V7 example_incomplete_ii7(HarmonyMode mode) {
    static const int s[2][4] = {{3, 0, 1, -6}, {3, -1, 1, -3}};
    return make(mode, s);
}

/* A complete ii7 to an incomplete V7 (sol twice, no re). */
static HarmonyIi7V7 example_incomplete_v7(HarmonyMode mode) {
    static const int s[2][4] = {{5, 3, 0, -6}, {4, 3, -1, -3}};
    return make(mode, s);
}

static bool holds(HarmonyIi7V7Rule rule, const HarmonyIi7V7 *p) {
    return harmony_ii7_v7_rule_holds(rule, p);
}

static void test_progression_is(void) {
    HarmonyChord good[2] = {CH7(2), CH7(5)};
    HarmonyChord triad_ii[2] = {CH(2), CH7(5)};
    HarmonyChord triad_v[2] = {CH7(2), CH(5)};
    HarmonyChord inverted_ii[2] = {CH7I(2, 1), CH7(5)};
    HarmonyChord inverted_v[2] = {CH7(2), CH7I(5, 1)};
    HarmonyChord bad[2] = {CH7(2), CH7(9)};
    assert(harmony_ii7_v7_progression_is(good));
    assert(!harmony_ii7_v7_progression_is(triad_ii));
    assert(!harmony_ii7_v7_progression_is(triad_v));
    assert(!harmony_ii7_v7_progression_is(inverted_ii));
    assert(!harmony_ii7_v7_progression_is(inverted_v));
    assert(!harmony_ii7_v7_progression_is(bad));
    assert(!harmony_ii7_v7_progression_is(NULL));
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_ii7_v7_rule, harmony_ii7_v7_constraint, HARMONY_II7_V7_RULE_COUNT);
    for (int i = 0; i < HARMONY_II7_V7_RULE_COUNT; i++) {
        assert(constraint_type(harmony_ii7_v7_constraint((HarmonyIi7V7Rule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_II7_V7_RULE_COUNT] = {"progression", "chord-notes", "one-incomplete",
                                                             "no-parallels"};
    for (int i = 0; i < HARMONY_II7_V7_RULE_COUNT; i++) {
        assert(strcmp(harmony_ii7_v7_rule_name((HarmonyIi7V7Rule)i), expected[i]) == 0);
    }
    assert(harmony_ii7_v7_rule_name(IV(RULE_COUNT)) == NULL);
}

static void test_either_chord_may_be_the_incomplete_one(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyIi7V7 examples[] = {example_incomplete_ii7((HarmonyMode)mode),
                                   example_incomplete_v7((HarmonyMode)mode)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            for (int rule = 0; rule < HARMONY_II7_V7_RULE_COUNT; rule++) {
                assert(holds((HarmonyIi7V7Rule)rule, &examples[i]));
            }
            unsigned hard = 99, soft = 99;
            assert(harmony_ii7_v7_check(&examples[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_both_complete_breaks_the_rule(void) {
    HarmonyIi7V7 p = example_incomplete_v7(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_SOPRANO] = 3; /* fa re ti sol: a complete V7 */
    p.steps[1][VOICE_PART_ALTO] = 1;
    assert(holds(IV(CHORD_NOTES), &p));
    assert(!holds(IV(ONE_INCOMPLETE), &p));
    unsigned hard = 0, soft = 0;
    assert(harmony_ii7_v7_check(&p, &hard, &soft));
    assert(hard == (1u << IV(ONE_INCOMPLETE)) && soft == 0);
}

static void test_progression(void) {
    HarmonyIi7V7 p = example_incomplete_ii7(HARMONY_MODE_MAJOR);
    p.chords[0] = CH(2);
    assert(!holds(IV(PROGRESSION), &p));
    assert(!holds(IV(CHORD_NOTES), &p));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyIi7V7 foreign = example_incomplete_ii7(HARMONY_MODE_MAJOR);
    foreign.steps[0][VOICE_PART_ALTO] = 4; /* sol is not in ii7 */
    HarmonyIi7V7 no_seventh = example_incomplete_v7(HARMONY_MODE_MAJOR);
    no_seventh.steps[1][VOICE_PART_ALTO] = 4; /* sol sol ti sol: no fa */
    HarmonyIi7V7 wrong_bass = example_incomplete_v7(HARMONY_MODE_MAJOR);
    wrong_bass.steps[1][VOICE_PART_BASS] = -4; /* fa in the bass of V7 */
    assert(!holds(IV(CHORD_NOTES), &foreign));
    assert(!holds(IV(CHORD_NOTES), &no_seventh));
    assert(!holds(IV(CHORD_NOTES), &wrong_bass));
}

static void test_no_parallel_fifths(void) {
    HarmonyIi7V7 p = example_incomplete_v7(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_SOPRANO] = 8; /* re over the bass sol, both up from a fifth */
    assert(!holds(IV(NO_PARALLELS), &p));
}

static void test_invalid_subject(void) {
    HarmonyIi7V7 bad_chord = example_incomplete_ii7(HARMONY_MODE_MAJOR);
    bad_chord.chords[1] = CH(9);
    HarmonyIi7V7 bad_mode = example_incomplete_ii7(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyIi7V7 good = example_incomplete_ii7(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_ii7_v7_valid(NULL));
    assert(!harmony_ii7_v7_valid(&bad_chord));
    assert(!harmony_ii7_v7_valid(&bad_mode));
    assert(!harmony_ii7_v7_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_ii7_v7_check(&good, NULL, &soft));
    assert(!harmony_ii7_v7_check(&good, &hard, NULL));
    assert(!holds(IV(PROGRESSION), &bad_chord));
    assert(!holds(IV(RULE_COUNT), &good));
}

int main(void) {
    test_progression_is();
    test_rules_reference_framework();
    test_names();
    test_either_chord_may_be_the_incomplete_one();
    test_both_complete_breaks_the_rule();
    test_progression();
    test_each_chord_has_its_own_notes();
    test_no_parallel_fifths();
    test_invalid_subject();
    printf("test_harmony_ii7_v7_rules: ok\n");
    return 0;
}
