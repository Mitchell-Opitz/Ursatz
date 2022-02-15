#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_harmonic_rules.h"
#include "century_rule_check.h"
#include "century_test_util.h"

#define HARMONIC(name) SIXTEENTH_CENTURY_HARMONIC_##name

static bool holds(SixteenthCenturyHarmonicRule rule, const SixteenthCenturyScore *score) {
    return sixteenth_century_harmonic_rule_holds(rule, score);
}

/* A score of voices of half notes: `rows` voices, each of `n` pitches. */
#define TWO_VOICES(name, upper, lower)                                                               \
    StandardCounterpointSlot name##_upper_slots[4 * COUNT_OF(upper)];                                \
    StandardCounterpointSlot name##_lower_slots[4 * COUNT_OF(lower)];                                \
    StandardCounterpointSlotLine name##_voices[] = {halves(name##_upper_slots, (upper), COUNT_OF(upper)), \
                                                    halves(name##_lower_slots, (lower), COUNT_OF(lower))}; \
    SCORE_OF(name, name##_voices)

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(sixteenth_century_harmonic_rule, sixteenth_century_harmonic_constraint,
                                    SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT);
    for (int i = 0; i < SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(sixteenth_century_harmonic_constraint((SixteenthCenturyHarmonicRule)i));
        assert(type == (i <= HARMONIC(DIMINISHED_TRIAD_RESOLVES) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(sixteenth_century_harmonic_rule_name(HARMONIC(NO_PARALLEL_PERFECTS)), "no-parallel-perfects") == 0);
    assert(strcmp(sixteenth_century_harmonic_rule_name(HARMONIC(DIMINISHED_TRIAD_RESOLVES)),
                  "diminished-triad-resolves") == 0);
    assert(strcmp(sixteenth_century_harmonic_rule_name(HARMONIC(NO_DIRECT_PERFECTS)), "no-direct-perfects") == 0);
    assert(strcmp(sixteenth_century_harmonic_rule_name(HARMONIC(FINAL_CHORD)), "final-chord") == 0);
    assert(sixteenth_century_harmonic_rule_name(HARMONIC(RULE_COUNT)) == NULL);
}

static void test_no_parallel_perfects(void) {
    /* Parallel fifths. */
    const PitchSpelling *fifths_up[] = {P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *fifths_low[] = {P(C, 0, 4), P(D, 0, 4)};
    TWO_VOICES(fifths, fifths_up, fifths_low);
    assert(!holds(HARMONIC(NO_PARALLEL_PERFECTS), &fifths));

    /* Parallel octaves. */
    const PitchSpelling *octaves_up[] = {P(C, 0, 5), P(D, 0, 5)};
    const PitchSpelling *octaves_low[] = {P(C, 0, 4), P(D, 0, 4)};
    TWO_VOICES(octaves, octaves_up, octaves_low);
    assert(!holds(HARMONIC(NO_PARALLEL_PERFECTS), &octaves));

    /* Unisons count as octaves. */
    const PitchSpelling *unisons_up[] = {P(C, 0, 4), P(D, 0, 4)};
    const PitchSpelling *unisons_low[] = {P(C, 0, 4), P(D, 0, 4)};
    TWO_VOICES(unisons, unisons_up, unisons_low);
    assert(!holds(HARMONIC(NO_PARALLEL_PERFECTS), &unisons));

    /* A diminished fifth followed by a perfect fifth is parallel too. */
    const PitchSpelling *dim_up[] = {P(F, 0, 4), P(G, 0, 4)};
    const PitchSpelling *dim_low[] = {P(B, 0, 3), P(C, 0, 4)};
    TWO_VOICES(dim, dim_up, dim_low);
    assert(!holds(HARMONIC(NO_PARALLEL_PERFECTS), &dim));

    /* A fifth that moves to a sixth, and a fifth in contrary motion to a
       fifth of another kind. */
    const PitchSpelling *sixth_up[] = {P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *sixth_low[] = {P(C, 0, 4), P(C, 0, 4)};
    TWO_VOICES(sixth, sixth_up, sixth_low);
    assert(holds(HARMONIC(NO_PARALLEL_PERFECTS), &sixth));

    /* Oblique: one voice holds. */
    const PitchSpelling *oblique_up[] = {P(G, 0, 4), P(C, 0, 5)};
    const PitchSpelling *oblique_low[] = {P(C, 0, 4), P(C, 0, 4)};
    TWO_VOICES(oblique, oblique_up, oblique_low);
    assert(holds(HARMONIC(NO_PARALLEL_PERFECTS), &oblique));

    /* A fifth to a twelfth in contrary motion. */
    const PitchSpelling *contrary_up[] = {P(G, 0, 4), P(D, 0, 5)};
    const PitchSpelling *contrary_low[] = {P(C, 0, 4), P(G, 0, 3)};
    TWO_VOICES(contrary, contrary_up, contrary_low);
    assert(holds(HARMONIC(NO_PARALLEL_PERFECTS), &contrary));
}

static void test_no_direct_perfects(void) {
    /* A fifth reached by similar motion, with more to come. */
    const PitchSpelling *direct_up[] = {P(E, 0, 4), P(A, 0, 4), P(C, 0, 5)};
    const PitchSpelling *direct_low[] = {P(C, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    TWO_VOICES(direct, direct_up, direct_low);
    assert(!holds(HARMONIC(NO_DIRECT_PERFECTS), &direct));

    /* The same arrival at the very end of the pair is free. */
    const PitchSpelling *cadence_up[] = {P(E, 0, 4), P(A, 0, 4)};
    const PitchSpelling *cadence_low[] = {P(C, 0, 4), P(D, 0, 4)};
    TWO_VOICES(cadence, cadence_up, cadence_low);
    assert(holds(HARMONIC(NO_DIRECT_PERFECTS), &cadence));

    /* A third voice in contrary motion excuses it. */
    const PitchSpelling *mitigated_top[] = {P(E, 0, 4), P(A, 0, 4), P(C, 0, 5)};
    const PitchSpelling *mitigated_mid[] = {P(C, 0, 4), P(D, 0, 4), P(C, 0, 4)};
    const PitchSpelling *mitigated_bass[] = {P(G, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    StandardCounterpointSlot top_slots[12], mid_slots[12], bass_slots[12];
    StandardCounterpointSlotLine voices[] = {halves(top_slots, mitigated_top, 3),
                                             halves(mid_slots, mitigated_mid, 3),
                                             halves(bass_slots, mitigated_bass, 3)};
    SCORE_OF(mitigated, voices);
    assert(holds(HARMONIC(NO_DIRECT_PERFECTS), &mitigated));
}

static void test_diminished_triad_resolves(void) {
    /* B, F, and D (the diminished triad in first inversion) resolve: ti up to
       do, fa down to mi, re down to do. */
    const PitchSpelling *top_good[] = {P(B, 0, 4), P(C, 0, 5)};
    const PitchSpelling *mid_good[] = {P(F, 0, 4), P(E, 0, 4)};
    const PitchSpelling *bass_good[] = {P(D, 0, 3), P(C, 0, 3)};
    StandardCounterpointSlot a_slots[8], b_slots[8], c_slots[8];
    StandardCounterpointSlotLine good_voices[] = {halves(a_slots, top_good, 2), halves(b_slots, mid_good, 2),
                                                  halves(c_slots, bass_good, 2)};
    SCORE_OF(good, good_voices);
    assert(holds(HARMONIC(DIMINISHED_TRIAD_RESOLVES), &good));

    /* The leading note is repeated instead of rising. */
    const PitchSpelling *top_bad[] = {P(B, 0, 4), P(B, 0, 4)};
    StandardCounterpointSlot d_slots[8];
    StandardCounterpointSlotLine bad_voices[] = {halves(d_slots, top_bad, 2), good_voices[1], good_voices[2]};
    SCORE_OF(bad, bad_voices);
    assert(!holds(HARMONIC(DIMINISHED_TRIAD_RESOLVES), &bad));

    /* The fifth rises instead of falling. */
    const PitchSpelling *mid_bad[] = {P(F, 0, 4), P(G, 0, 4)};
    StandardCounterpointSlot e_slots[8];
    StandardCounterpointSlotLine mid_voices[] = {good_voices[0], halves(e_slots, mid_bad, 2), good_voices[2]};
    SCORE_OF(mid, mid_voices);
    assert(!holds(HARMONIC(DIMINISHED_TRIAD_RESOLVES), &mid));

    /* A triad at the very end has nothing to resolve to; two voices cannot
       make a triad. */
    const PitchSpelling *end_top[] = {P(B, 0, 4)};
    const PitchSpelling *end_mid[] = {P(F, 0, 4)};
    const PitchSpelling *end_bass[] = {P(D, 0, 3)};
    StandardCounterpointSlot f_slots[4], g_slots[4], h_slots[4];
    StandardCounterpointSlotLine end_voices[] = {halves(f_slots, end_top, 1), halves(g_slots, end_mid, 1),
                                                 halves(h_slots, end_bass, 1)};
    SCORE_OF(end, end_voices);
    assert(holds(HARMONIC(DIMINISHED_TRIAD_RESOLVES), &end));
    TWO_VOICES(two, top_good, mid_good);
    assert(holds(HARMONIC(DIMINISHED_TRIAD_RESOLVES), &two));
}

static void test_final_chord(void) {
    const PitchSpelling *top[] = {P(E, 0, 4)};
    const PitchSpelling *mid[] = {P(G, 0, 3)};
    const PitchSpelling *bass[] = {P(C, 0, 3)};
    StandardCounterpointSlot a[4], b[4], c[4];
    StandardCounterpointSlotLine major_voices[] = {halves(a, top, 1), halves(b, mid, 1), halves(c, bass, 1)};
    SCORE_OF(major, major_voices);
    assert(holds(HARMONIC(FINAL_CHORD), &major));

    const PitchSpelling *minor_top[] = {P(E, -1, 4)};
    StandardCounterpointSlot d[4];
    StandardCounterpointSlotLine minor_voices[] = {halves(d, minor_top, 1), major_voices[1], major_voices[2]};
    SCORE_OF(minor, minor_voices);
    assert(!holds(HARMONIC(FINAL_CHORD), &minor));

    /* A bare fifth. */
    const PitchSpelling *fifth_top[] = {P(G, 0, 3)};
    const PitchSpelling *fifth_bass[] = {P(C, 0, 3)};
    TWO_VOICES(fifth, fifth_top, fifth_bass);
    assert(holds(HARMONIC(FINAL_CHORD), &fifth));

    /* A sixth over the bass. */
    const PitchSpelling *sixth_top[] = {P(A, 0, 3)};
    TWO_VOICES(sixth, sixth_top, fifth_bass);
    assert(!holds(HARMONIC(FINAL_CHORD), &sixth));

    /* A rest in the last slot is not judged. */
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot sounding[] = {ATK(c4), TIE(c4)};
    StandardCounterpointSlot resting[] = {ATK(P(G, 0, 3)), REST()};
    StandardCounterpointSlotLine rest_voices[] = {{sounding, 2}, {resting, 2}};
    SCORE_OF(rested, rest_voices);
    assert(holds(HARMONIC(FINAL_CHORD), &rested));
}

static void test_invalid(void) {
    const PitchSpelling *up[] = {P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *low[] = {P(C, 0, 4), P(D, 0, 4)};
    TWO_VOICES(score, up, low);
    assert(!holds(HARMONIC(RULE_COUNT), &score));
    assert(!holds((SixteenthCenturyHarmonicRule)-1, &score));
    assert(!holds(HARMONIC(FINAL_CHORD), NULL));
    unsigned hard = 7, soft = 7;
    assert(!sixteenth_century_harmonic_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!sixteenth_century_harmonic_check(&score, NULL, &soft));
    assert(!sixteenth_century_harmonic_check(&score, &hard, NULL));
}

static void test_check(void) {
    const PitchSpelling *up[] = {P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *low[] = {P(C, 0, 4), P(D, 0, 4)};
    TWO_VOICES(score, up, low);
    unsigned hard = 0, soft = 0;
    assert(sixteenth_century_harmonic_check(&score, &hard, &soft));
    assert(hard == (1u << HARMONIC(NO_PARALLEL_PERFECTS)));
    assert(soft == 0);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_no_parallel_perfects();
    test_no_direct_perfects();
    test_diminished_triad_resolves();
    test_final_chord();
    test_invalid();
    test_check();
    printf("test_century_harmonic_rules: ok\n");
    return 0;
}
