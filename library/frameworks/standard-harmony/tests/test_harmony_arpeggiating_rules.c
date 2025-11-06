#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_arpeggiating_rules.h"
#include "harmony_rule_check.h"
#include "harmony_six_four_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "arpeggiating", HARMONY_ARPEGGIATING_RULE_COUNT);
#define harmony_arpeggiating_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_arpeggiating_constraint, harmony_arpeggiating_preference, harmony_arpeggiating_rule_holds, HarmonySixFour, which, subject)

#define ARP(name) HARMONY_ARPEGGIATING_##name

static bool holds(HarmonyArpeggiatingRule rule, const HarmonySixFour *s) {
    return harmony_arpeggiating_rule_holds(rule, s);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_arpeggiating_rule, harmony_arpeggiating_constraint,
                                    HARMONY_ARPEGGIATING_RULE_COUNT);
    for (int i = 0; i < HARMONY_ARPEGGIATING_RULE_COUNT; i++) {
        assert(constraint_type(harmony_arpeggiating_constraint((HarmonyArpeggiatingRule)i)) == CONSTRAINT_TYPE_HARD);
    }
    assert(strcmp(harmony_arpeggiating_rule_name(ARP(IS_SIX_FOUR)), "is-six-four") == 0);
    assert(strcmp(harmony_arpeggiating_rule_name(ARP(SAME_CHORD)), "same-chord") == 0);
    assert(strcmp(harmony_arpeggiating_rule_name(ARP(BASS_LEAPS_TO_FIFTH)), "bass-leaps-to-fifth") == 0);
    assert(strcmp(harmony_arpeggiating_rule_name(ARP(UPPER_VOICES_SUSTAIN)), "upper-voices-sustain") == 0);
    assert(harmony_arpeggiating_rule_name(ARP(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_breaks_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonySixFour s = example_arpeggiating((HarmonyMode)mode);
        for (int rule = 0; rule < HARMONY_ARPEGGIATING_RULE_COUNT; rule++) {
            assert(holds((HarmonyArpeggiatingRule)rule, &s));
        }
        unsigned hard = 99, soft = 99;
        assert(harmony_arpeggiating_check(&s, &hard, &soft));
        assert(hard == 0 && soft == 0);
    }
}

static void test_the_same_chord(void) {
    HarmonySixFour changes = example_arpeggiating(HARMONY_MODE_MAJOR);
    changes.chords[2] = CH(4);
    HarmonySixFour changes_first = example_arpeggiating(HARMONY_MODE_MAJOR);
    changes_first.chords[0] = CH(5);
    assert(!holds(ARP(SAME_CHORD), &changes));
    assert(!holds(ARP(SAME_CHORD), &changes_first));
}

static void test_the_bass_leaps_to_the_fifth(void) {
    /* A step to the fifth is a passing tone, not an arpeggiation. */
    HarmonySixFour step = example_arpeggiating(HARMONY_MODE_MAJOR);
    step.steps[0][VOICE_PART_BASS] = -4; /* fa, then sol */
    assert(!holds(ARP(BASS_LEAPS_TO_FIFTH), &step));
    /* A leap to a note that is not the fifth: the root an octave up. */
    HarmonySixFour not_fifth = example_arpeggiating(HARMONY_MODE_MAJOR);
    not_fifth.steps[1][VOICE_PART_BASS] = 0;
    assert(!holds(ARP(BASS_LEAPS_TO_FIFTH), &not_fifth));
    /* The bass staying on the fifth. */
    HarmonySixFour stays = example_arpeggiating(HARMONY_MODE_MAJOR);
    stays.steps[0][VOICE_PART_BASS] = -3;
    assert(!holds(ARP(BASS_LEAPS_TO_FIFTH), &stays));
    /* The leap may be down as well. */
    HarmonySixFour down = example_arpeggiating(HARMONY_MODE_MAJOR);
    down.steps[0][VOICE_PART_BASS] = 4;
    assert(holds(ARP(BASS_LEAPS_TO_FIFTH), &down));
}

static void test_the_upper_voices_sustain(void) {
    HarmonySixFour moves_in = example_arpeggiating(HARMONY_MODE_MAJOR);
    moves_in.steps[1][VOICE_PART_TENOR] = 1;
    HarmonySixFour moves_out = example_arpeggiating(HARMONY_MODE_MAJOR);
    moves_out.steps[2][VOICE_PART_ALTO] = 3;
    assert(!holds(ARP(UPPER_VOICES_SUSTAIN), &moves_in));
    assert(!holds(ARP(UPPER_VOICES_SUSTAIN), &moves_out));
    /* It is a hard rule. */
    unsigned hard = 0, soft = 99;
    assert(harmony_arpeggiating_check(&moves_in, &hard, &soft));
    assert((hard & (1u << ARP(UPPER_VOICES_SUSTAIN))) != 0 && soft == 0);
}

static void test_not_a_six_four(void) {
    HarmonySixFour s = example_arpeggiating(HARMONY_MODE_MAJOR);
    s.chords[1] = CH(1);
    assert(!holds(ARP(IS_SIX_FOUR), &s));
}

static void test_invalid_subject(void) {
    HarmonySixFour bad_mode = example_arpeggiating(HARMONY_MODE_COUNT);
    HarmonySixFour bad_chord = example_arpeggiating(HARMONY_MODE_MAJOR);
    bad_chord.chords[2] = CH(0);
    HarmonySixFour *bad[] = {&bad_mode, &bad_chord};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        for (int rule = 0; rule < HARMONY_ARPEGGIATING_RULE_COUNT; rule++) {
            assert(!holds((HarmonyArpeggiatingRule)rule, bad[i]));
        }
        assert(!harmony_arpeggiating_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    HarmonySixFour ok = example_arpeggiating(HARMONY_MODE_MAJOR);
    assert(!holds(ARP(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_the_worked_example_breaks_no_rule();
    test_the_same_chord();
    test_the_bass_leaps_to_the_fifth();
    test_the_upper_voices_sustain();
    test_not_a_six_four();
    test_invalid_subject();
    printf("test_harmony_arpeggiating_rules: ok\n");
    return 0;
}
