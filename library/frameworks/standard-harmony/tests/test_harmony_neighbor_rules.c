#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_neighbor_rules.h"
#include "harmony_rule_check.h"
#include "harmony_six_four_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "neighbor", HARMONY_NEIGHBOR_RULE_COUNT);
#define harmony_neighbor_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_neighbor_constraint, harmony_neighbor_rule_holds, HarmonySixFour, which, subject)

#define NBR(name) HARMONY_NEIGHBOR_##name

static bool holds(HarmonyNeighborRule rule, const HarmonySixFour *s) {
    return harmony_neighbor_rule_holds(rule, s);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_neighbor_rule, harmony_neighbor_constraint, HARMONY_NEIGHBOR_RULE_COUNT);
    for (int i = 0; i < HARMONY_NEIGHBOR_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_neighbor_constraint((HarmonyNeighborRule)i));
        assert(type == (i <= NBR(UPPER_NEIGHBORS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_NEIGHBOR_RULE_COUNT] = {
        "is-six-four", "static-bass", "same-outer-chords", "bass-is-root", "six-four-notes", "upper-neighbors",
        "prolongs-i-or-v"};
    for (int i = 0; i < HARMONY_NEIGHBOR_RULE_COUNT; i++) {
        assert(strcmp(harmony_neighbor_rule_name((HarmonyNeighborRule)i), expected[i]) == 0);
    }
    assert(harmony_neighbor_rule_name(NBR(RULE_COUNT)) == NULL);
}

static void test_the_worked_examples_break_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonySixFour examples[] = {example_neighbor_tonic(m), example_neighbor_static_soprano(m),
                                     example_neighbor_dominant(m)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            for (int rule = 0; rule < HARMONY_NEIGHBOR_RULE_COUNT; rule++) {
                assert(holds((HarmonyNeighborRule)rule, &examples[i]));
            }
            unsigned hard = 99, soft = 99;
            assert(harmony_neighbor_check(&examples[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_static_bass(void) {
    HarmonySixFour moves = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    moves.steps[1][VOICE_PART_BASS] = -6;
    HarmonySixFour moves_last = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    moves_last.steps[2][VOICE_PART_BASS] = -14;
    assert(!holds(NBR(STATIC_BASS), &moves));
    assert(!holds(NBR(STATIC_BASS), &moves_last));
}

static void test_the_outer_chords_are_the_same_in_root_position(void) {
    HarmonySixFour inverted = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    inverted.chords[2] = CHI(1, 1);
    HarmonySixFour different = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    different.chords[2] = CH(5);
    HarmonySixFour seventh = example_neighbor_dominant(HARMONY_MODE_MAJOR);
    seventh.chords[0] = CH7(5);
    seventh.chords[2] = CH7(5);
    assert(!holds(NBR(SAME_OUTER_CHORDS), &inverted));
    assert(!holds(NBR(SAME_OUTER_CHORDS), &different));
    assert(!holds(NBR(SAME_OUTER_CHORDS), &seventh));
}

static void test_the_bass_is_the_root_of_the_outer_chords(void) {
    /* IV6/4 over do goes with I; over do it does not go with V. */
    HarmonySixFour wrong_chord = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    wrong_chord.chords[0] = CH(5);
    wrong_chord.chords[2] = CH(5);
    assert(!holds(NBR(BASS_IS_ROOT), &wrong_chord));
    HarmonySixFour ok = example_neighbor_dominant(HARMONY_MODE_MAJOR);
    assert(holds(NBR(BASS_IS_ROOT), &ok));
}

static void test_six_four_notes(void) {
    HarmonySixFour no_double = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    no_double.steps[1][VOICE_PART_TENOR] = -3; /* sol: not doubling do, and not la or fa */
    assert(!holds(NBR(SIX_FOUR_NOTES), &no_double));
}

static void test_upper_neighbors(void) {
    /* The soprano and the alto go up a step and back, the tenor stays. */
    HarmonySixFour ok = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    assert(holds(NBR(UPPER_NEIGHBORS), &ok));
    /* A lower neighbor is not an upper one. */
    HarmonySixFour lower = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    lower.steps[1][VOICE_PART_SOPRANO] = 3;
    assert(!holds(NBR(UPPER_NEIGHBORS), &lower));
    /* A voice that goes up a step and does not come back. */
    HarmonySixFour stays_up = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    stays_up.steps[2][VOICE_PART_ALTO] = 3;
    assert(!holds(NBR(UPPER_NEIGHBORS), &stays_up));
    /* Only one neighbor, or all three. */
    HarmonySixFour one = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    one.steps[1][VOICE_PART_ALTO] = 2;
    assert(!holds(NBR(UPPER_NEIGHBORS), &one));
    HarmonySixFour three = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    three.steps[1][VOICE_PART_TENOR] = 1;
    assert(!holds(NBR(UPPER_NEIGHBORS), &three));
    /* A neighbor that leaps. */
    HarmonySixFour leaps = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    leaps.steps[1][VOICE_PART_SOPRANO] = 6;
    assert(!holds(NBR(UPPER_NEIGHBORS), &leaps));
}

static void test_prolongs_i_or_v(void) {
    /* A neighbor 6/4 on another chord (here ii over a static bass) is possible, not usual. */
    HarmonySixFour on_ii = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    on_ii.chords[0] = CH(2);
    on_ii.chords[2] = CH(2);
    assert(!holds(NBR(PROLONGS_I_OR_V), &on_ii));
    unsigned hard = 99, soft = 0;
    assert(harmony_neighbor_check(&on_ii, &hard, &soft));
    assert((soft & (1u << NBR(PROLONGS_I_OR_V))) != 0);
}

static void test_invalid_subject(void) {
    HarmonySixFour bad_mode = example_neighbor_tonic(HARMONY_MODE_COUNT);
    HarmonySixFour bad_beat = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    bad_beat.beat[0] = BEAT_STRENGTH_COUNT;
    HarmonySixFour *bad[] = {&bad_mode, &bad_beat};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        for (int rule = 0; rule < HARMONY_NEIGHBOR_RULE_COUNT; rule++) {
            assert(!holds((HarmonyNeighborRule)rule, bad[i]));
        }
        assert(!harmony_neighbor_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    HarmonySixFour ok = example_neighbor_tonic(HARMONY_MODE_MAJOR);
    assert(!holds(NBR(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_examples_break_no_rule();
    test_static_bass();
    test_the_outer_chords_are_the_same_in_root_position();
    test_the_bass_is_the_root_of_the_outer_chords();
    test_six_four_notes();
    test_upper_neighbors();
    test_prolongs_i_or_v();
    test_invalid_subject();
    printf("test_harmony_neighbor_rules: ok\n");
    return 0;
}
