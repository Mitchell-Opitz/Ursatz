#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_melodic_rules.h"
#include "century_rule_check.h"
#include "century_test_util.h"

#define MELODIC(name) SIXTEENTH_CENTURY_MELODIC_##name

static bool holds(SixteenthCenturyMelodicRule rule, const SixteenthCenturyScore *score) {
    return sixteenth_century_melodic_rule_holds(rule, score);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(sixteenth_century_melodic_rule, sixteenth_century_melodic_constraint,
                                    SIXTEENTH_CENTURY_MELODIC_RULE_COUNT);
    for (int i = 0; i < SIXTEENTH_CENTURY_MELODIC_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(sixteenth_century_melodic_constraint((SixteenthCenturyMelodicRule)i));
        assert(type == (i <= MELODIC(RANGE_LIMIT) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(NO_DISSONANT_LEAPS)), "no-dissonant-leaps") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(NO_OUTLINED_DISSONANCE)), "no-outlined-dissonance") ==
           0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(RANGE_LIMIT)), "range-limit") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(SIXTHS)), "sixths") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(LEAP_ORDER)), "leap-order") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(LEAP_RECOVERY)), "leap-recovery") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(NO_TRIAD_OUTLINE)), "no-triad-outline") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(ARCH)), "arch") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(RANGE_PREFERRED)), "range-preferred") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(FINAL_STEP)), "final-step") == 0);
    assert(strcmp(sixteenth_century_melodic_rule_name(MELODIC(LEADING_NOTE)), "leading-note") == 0);
    assert(sixteenth_century_melodic_rule_name(MELODIC(RULE_COUNT)) == NULL);
}

static void test_good_melody(void) {
    const PitchSpelling *tonic = P(D, 0, 4);
    const PitchSpelling *pitches[] = {P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4),
                                      P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    MELODY(score, pitches, tonic, false);
    for (int i = 0; i < SIXTEENTH_CENTURY_MELODIC_RULE_COUNT; i++) {
        assert(holds((SixteenthCenturyMelodicRule)i, &score));
    }
}

static void test_no_dissonant_leaps(void) {
    const PitchSpelling *tritone[] = {P(F, 0, 4), P(B, 0, 4)};
    MELODY(tritone_score, tritone, NULL, false);
    assert(!holds(MELODIC(NO_DISSONANT_LEAPS), &tritone_score));

    const PitchSpelling *seventh[] = {P(D, 0, 4), P(C, 0, 5)};
    MELODY(seventh_score, seventh, NULL, false);
    assert(!holds(MELODIC(NO_DISSONANT_LEAPS), &seventh_score));

    const PitchSpelling *fourth[] = {P(E, 0, 4), P(A, 0, 4), P(G, 0, 4)};
    MELODY(fourth_score, fourth, NULL, false);
    assert(holds(MELODIC(NO_DISSONANT_LEAPS), &fourth_score));
}

static void test_no_outlined_dissonance(void) {
    /* Steps, yet the run F to B is an augmented fourth. */
    const PitchSpelling *tritone[] = {P(F, 0, 4), P(G, 0, 4), P(A, 0, 4), P(B, 0, 4)};
    MELODY(tritone_score, tritone, NULL, false);
    assert(holds(MELODIC(NO_DISSONANT_LEAPS), &tritone_score));
    assert(!holds(MELODIC(NO_OUTLINED_DISSONANCE), &tritone_score));

    /* A run that outlines a seventh. */
    const PitchSpelling *seventh[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4),
                                      P(G, 0, 4), P(A, 0, 4), P(B, 0, 4)};
    MELODY(seventh_score, seventh, NULL, false);
    assert(!holds(MELODIC(NO_OUTLINED_DISSONANCE), &seventh_score));

    /* A run that outlines a fourth. */
    const PitchSpelling *fourth[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    MELODY(fourth_score, fourth, NULL, false);
    assert(holds(MELODIC(NO_OUTLINED_DISSONANCE), &fourth_score));
}

static void test_range_limit(void) {
    const PitchSpelling *twelfth[] = {P(C, 0, 4), P(G, 0, 5)};
    MELODY(twelfth_score, twelfth, NULL, false);
    assert(holds(MELODIC(RANGE_LIMIT), &twelfth_score));

    const PitchSpelling *thirteenth[] = {P(C, 0, 4), P(A, 0, 5)};
    MELODY(thirteenth_score, thirteenth, NULL, false);
    assert(!holds(MELODIC(RANGE_LIMIT), &thirteenth_score));
}

static void test_sixths(void) {
    const PitchSpelling *major_up[] = {P(C, 0, 4), P(A, 0, 4)};
    MELODY(major_up_score, major_up, NULL, false);
    assert(!holds(MELODIC(SIXTHS), &major_up_score));

    const PitchSpelling *minor_up[] = {P(E, 0, 4), P(C, 0, 5)};
    MELODY(minor_up_score, minor_up, NULL, false);
    assert(holds(MELODIC(SIXTHS), &minor_up_score));

    const PitchSpelling *minor_down[] = {P(C, 0, 5), P(E, 0, 4)};
    MELODY(minor_down_score, minor_down, NULL, false);
    assert(!holds(MELODIC(SIXTHS), &minor_down_score));
}

static void test_leap_order(void) {
    /* Rising: a fourth then a fifth puts the larger leap higher. */
    const PitchSpelling *rising_bad[] = {P(C, 0, 4), P(F, 0, 4), P(C, 0, 5)};
    MELODY(rising_bad_score, rising_bad, NULL, false);
    assert(!holds(MELODIC(LEAP_ORDER), &rising_bad_score));

    const PitchSpelling *rising_good[] = {P(C, 0, 4), P(G, 0, 4), P(C, 0, 5)};
    MELODY(rising_good_score, rising_good, NULL, false);
    assert(holds(MELODIC(LEAP_ORDER), &rising_good_score));

    /* Falling: the second leap, the lower one, must be the larger. */
    const PitchSpelling *falling_good[] = {P(C, 0, 5), P(G, 0, 4), P(C, 0, 4)};
    MELODY(falling_good_score, falling_good, NULL, false);
    assert(holds(MELODIC(LEAP_ORDER), &falling_good_score));

    const PitchSpelling *falling_worse[] = {P(C, 0, 5), P(F, 0, 4), P(C, 0, 4)};
    MELODY(falling_worse_score, falling_worse, NULL, false);
    assert(!holds(MELODIC(LEAP_ORDER), &falling_worse_score));

    /* A leap and a step are not successive leaps. */
    const PitchSpelling *step[] = {P(C, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    MELODY(step_score, step, NULL, false);
    assert(holds(MELODIC(LEAP_ORDER), &step_score));
}

static void test_leap_recovery(void) {
    const PitchSpelling *unrecovered[] = {P(C, 0, 4), P(G, 0, 4), P(A, 0, 4)};
    MELODY(unrecovered_score, unrecovered, NULL, false);
    assert(!holds(MELODIC(LEAP_RECOVERY), &unrecovered_score));

    const PitchSpelling *recovered[] = {P(C, 0, 4), P(G, 0, 4), P(F, 0, 4)};
    MELODY(recovered_score, recovered, NULL, false);
    assert(holds(MELODIC(LEAP_RECOVERY), &recovered_score));

    /* A leap of a fourth need not be recovered. */
    const PitchSpelling *fourth[] = {P(C, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    MELODY(fourth_score, fourth, NULL, false);
    assert(holds(MELODIC(LEAP_RECOVERY), &fourth_score));
}

static void test_no_triad_outline(void) {
    const PitchSpelling *triad[] = {P(C, 0, 4), P(E, 0, 4), P(G, 0, 4)};
    MELODY(triad_score, triad, NULL, false);
    assert(!holds(MELODIC(NO_TRIAD_OUTLINE), &triad_score));

    const PitchSpelling *scale[] = {P(C, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    MELODY(scale_score, scale, NULL, false);
    assert(holds(MELODIC(NO_TRIAD_OUTLINE), &scale_score));
}

static void test_arch(void) {
    const PitchSpelling *climax_last[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    MELODY(climax_last_score, climax_last, NULL, false);
    assert(!holds(MELODIC(ARCH), &climax_last_score));

    const PitchSpelling *arch[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    MELODY(arch_score, arch, NULL, false);
    assert(holds(MELODIC(ARCH), &arch_score));

    /* A short voice is not judged. */
    const PitchSpelling *short_voice[] = {P(C, 0, 4), P(D, 0, 4)};
    MELODY(short_score, short_voice, NULL, false);
    assert(holds(MELODIC(ARCH), &short_score));
}

static void test_range_preferred(void) {
    const PitchSpelling *within[] = {P(C, 0, 4), P(D, 0, 5)};
    MELODY(within_score, within, NULL, false);
    assert(holds(MELODIC(RANGE_PREFERRED), &within_score));

    const PitchSpelling *wide[] = {P(C, 0, 4), P(E, 0, 5)};
    MELODY(wide_score, wide, NULL, false);
    assert(!holds(MELODIC(RANGE_PREFERRED), &wide_score));
}

static void test_final_step(void) {
    const PitchSpelling *step[] = {P(C, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    MELODY(step_score, step, NULL, false);
    assert(holds(MELODIC(FINAL_STEP), &step_score));

    const PitchSpelling *leap[] = {P(C, 0, 4), P(G, 0, 4), P(D, 0, 4)};
    MELODY(leap_score, leap, NULL, false);
    assert(!holds(MELODIC(FINAL_STEP), &leap_score));
}

static void test_leading_note(void) {
    const PitchSpelling *tonic = P(D, 0, 4);

    /* C natural to D: a whole step below the final. */
    const PitchSpelling *natural[] = {P(E, 0, 4), P(C, 0, 4), P(D, 0, 4)};
    MELODY(natural_score, natural, tonic, false);
    assert(!holds(MELODIC(LEADING_NOTE), &natural_score));

    /* Raised to C sharp. */
    const PitchSpelling *raised[] = {P(E, 0, 4), P(C, 1, 4), P(D, 0, 4)};
    MELODY(raised_score, raised, tonic, false);
    assert(holds(MELODIC(LEADING_NOTE), &raised_score));

    /* Phrygian modes keep the whole step. */
    MELODY(phrygian_score, natural, tonic, true);
    assert(holds(MELODIC(LEADING_NOTE), &phrygian_score));

    /* Approach from above, and an unknown tonic, are not judged. */
    const PitchSpelling *above[] = {P(F, 0, 4), P(E, 0, 4), P(D, 0, 4)};
    MELODY(above_score, above, tonic, false);
    assert(holds(MELODIC(LEADING_NOTE), &above_score));
    MELODY(no_tonic_score, natural, NULL, false);
    assert(holds(MELODIC(LEADING_NOTE), &no_tonic_score));
}

static void test_every_voice_is_judged(void) {
    const PitchSpelling *good[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *bad[] = {P(F, 0, 4), P(B, 0, 4)};
    StandardCounterpointSlot good_slots[8];
    StandardCounterpointSlot bad_slots[8];
    StandardCounterpointSlotLine voices[] = {halves(good_slots, good, 2), halves(bad_slots, bad, 2)};
    SCORE_OF(score, voices);
    assert(!holds(MELODIC(NO_DISSONANT_LEAPS), &score));
}

static void test_invalid(void) {
    const PitchSpelling *pitches[] = {P(C, 0, 4), P(D, 0, 4)};
    MELODY(score, pitches, NULL, false);
    assert(!holds(MELODIC(RULE_COUNT), &score));
    assert(!holds((SixteenthCenturyMelodicRule)-1, &score));
    assert(!holds(MELODIC(RANGE_LIMIT), NULL));
    unsigned hard = 7, soft = 7;
    assert(!sixteenth_century_melodic_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!sixteenth_century_melodic_check(&score, NULL, &soft));
    assert(!sixteenth_century_melodic_check(&score, &hard, NULL));
}

static void test_check(void) {
    const PitchSpelling *pitches[] = {P(F, 0, 4), P(B, 0, 4), P(C, 0, 5)};
    MELODY(score, pitches, NULL, false);
    unsigned hard = 0, soft = 0;
    assert(sixteenth_century_melodic_check(&score, &hard, &soft));
    assert(hard & (1u << MELODIC(NO_DISSONANT_LEAPS)));
    assert(!(hard & (1u << MELODIC(RANGE_LIMIT))));

    const PitchSpelling *good[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    MELODY(good_score, good, NULL, false);
    assert(sixteenth_century_melodic_check(&good_score, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_melody();
    test_no_dissonant_leaps();
    test_no_outlined_dissonance();
    test_range_limit();
    test_sixths();
    test_leap_order();
    test_leap_recovery();
    test_no_triad_outline();
    test_arch();
    test_range_preferred();
    test_final_step();
    test_leading_note();
    test_every_voice_is_judged();
    test_invalid();
    test_check();
    printf("test_century_melodic_rules: ok\n");
    return 0;
}
