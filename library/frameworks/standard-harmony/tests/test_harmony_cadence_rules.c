#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_cadence_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"

static HarmonyCadence cadence(HarmonyChord from, HarmonyChord to, int soprano) {
    return (HarmonyCadence){from, to, soprano, false};
}

static CadenceType classify(HarmonyCadence c) {
    CadenceType type = (CadenceType)-1;
    assert(harmony_cadence_classify(&c, &type));
    return type;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_cadence_rule, harmony_cadence_constraint, HARMONY_CADENCE_RULE_COUNT);
    assert(constraint_type(harmony_cadence_constraint(HARMONY_CADENCE_CADENTIAL_PROGRESSION)) == CONSTRAINT_TYPE_HARD);
    assert(strcmp(harmony_cadence_rule_name(HARMONY_CADENCE_CADENTIAL_PROGRESSION), "cadential-progression") == 0);
    assert(harmony_cadence_rule_name(HARMONY_CADENCE_RULE_COUNT) == NULL);
}

static void test_perfect_authentic(void) {
    /* Both chords in root position, do in the soprano over the tonic. */
    assert(classify(cadence(CH(5), CH(1), 1)) == CADENCE_TYPE_PERFECT_AUTHENTIC);
}

static void test_imperfect_authentic(void) {
    /* mi or sol in the soprano over the tonic. */
    assert(classify(cadence(CH(5), CH(1), 3)) == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
    assert(classify(cadence(CH(5), CH(1), 5)) == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
    /* V, I, or both inverted, even with do in the soprano. */
    assert(classify(cadence(CHI(5, 1), CH(1), 1)) == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
    assert(classify(cadence(CH(5), CHI(1, 1), 1)) == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
    assert(classify(cadence(CHI(5, 1), CHI(1, 1), 1)) == CADENCE_TYPE_IMPERFECT_AUTHENTIC);
    /* A seventh chord on V in root position keeps the perfect cadence. */
    assert(classify(cadence(CH(5), CH(1), 1)) == CADENCE_TYPE_PERFECT_AUTHENTIC);
}

static void test_half(void) {
    /* x to V, whatever x is, whatever the soprano and inversion of V. */
    for (int degree = 1; degree <= 7; degree++) {
        assert(classify(cadence(CH(degree), CH(5), 7)) == CADENCE_TYPE_HALF);
    }
    assert(classify(cadence(CH(1), CH(5), 2)) == CADENCE_TYPE_HALF);
    assert(classify(cadence(CH(4), CHI(5, 1), 5)) == CADENCE_TYPE_HALF);
    assert(classify(cadence(LEADING_TONE_CHORD, CH(5), 2)) == CADENCE_TYPE_HALF);
}

static void test_not_a_cadence(void) {
    CadenceType type = CADENCE_TYPE_HALF;
    HarmonyCadence plagal = cadence(CH(4), CH(1), 1);
    HarmonyCadence deceptive = cadence(CH(5), CH(6), 1);
    HarmonyCadence leading_tone_to_tonic = cadence(LEADING_TONE_CHORD, CH(1), 1);
    HarmonyCadence tonic_to_predominant = cadence(CH(1), CH(2), 1);
    assert(!harmony_cadence_classify(&plagal, &type));
    assert(!harmony_cadence_classify(&deceptive, &type));
    assert(!harmony_cadence_classify(&leading_tone_to_tonic, &type));
    assert(!harmony_cadence_classify(&tonic_to_predominant, &type));
    assert(type == CADENCE_TYPE_HALF);

    assert(!harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, &plagal));
    assert(!harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, &deceptive));
    assert(!harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, &leading_tone_to_tonic));
}

static void test_cadential_progression_rule(void) {
    HarmonyCadence authentic = cadence(CH(5), CH(1), 3);
    HarmonyCadence half = cadence(CH(2), CH(5), 2);
    assert(harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, &authentic));
    assert(harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, &half));

    unsigned hard = 99, soft = 99;
    assert(harmony_cadence_check(&authentic, &hard, &soft));
    assert(hard == 0 && soft == 0);
    HarmonyCadence plagal = cadence(CH(4), CH(1), 1);
    assert(harmony_cadence_check(&plagal, &hard, &soft));
    assert(hard == (1u << HARMONY_CADENCE_CADENTIAL_PROGRESSION) && soft == 0);
}

static void test_subverted(void) {
    /* A potential cadence whose following music repeats mid-phrase material. */
    HarmonyCadence potential_iac = cadence(CHI(5, 1), CH(1), 3);
    potential_iac.subverted = true;
    HarmonyCadence potential_hc = cadence(CH(4), CH(5), 7);
    potential_hc.subverted = true;
    assert(classify(potential_iac) == CADENCE_TYPE_SUBVERTED);
    assert(classify(potential_hc) == CADENCE_TYPE_SUBVERTED);

    /* The flag does not make a non-cadence a cadence. */
    HarmonyCadence plagal = cadence(CH(4), CH(1), 1);
    plagal.subverted = true;
    CadenceType type = CADENCE_TYPE_HALF;
    assert(!harmony_cadence_classify(&plagal, &type));
}

static void test_strength_order(void) {
    assert(harmony_cadence_strength(CADENCE_TYPE_SUBVERTED) == 0);
    assert(harmony_cadence_strength(CADENCE_TYPE_HALF) == 1);
    assert(harmony_cadence_strength(CADENCE_TYPE_IMPERFECT_AUTHENTIC) == 2);
    assert(harmony_cadence_strength(CADENCE_TYPE_PERFECT_AUTHENTIC) == 3);
    assert(harmony_cadence_strength(CADENCE_TYPE_HALF) < harmony_cadence_strength(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(harmony_cadence_strength(CADENCE_TYPE_IMPERFECT_AUTHENTIC) <
           harmony_cadence_strength(CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(harmony_cadence_strength(CADENCE_TYPE_COUNT) == -1);
}

static void test_invalid_cadence(void) {
    HarmonyCadence bad_chord = cadence(CH(0), CH(1), 1);
    HarmonyCadence bad_to = cadence(CH(5), CH(9), 1);
    HarmonyCadence low_soprano = cadence(CH(5), CH(1), 0);
    HarmonyCadence high_soprano = cadence(CH(5), CH(1), 8);
    HarmonyCadence *bad[] = {&bad_chord, &bad_to, &low_soprano, &high_soprano};
    CadenceType type = CADENCE_TYPE_HALF;
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_cadence_valid(bad[i]));
        assert(!harmony_cadence_classify(bad[i], &type));
        assert(!harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, bad[i]));
        assert(!harmony_cadence_check(bad[i], &hard, &soft));
    }
    assert(type == CADENCE_TYPE_HALF && hard == 7 && soft == 7);
    assert(!harmony_cadence_valid(NULL));
    HarmonyCadence ok = cadence(CH(5), CH(1), 1);
    assert(!harmony_cadence_classify(&ok, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_perfect_authentic();
    test_imperfect_authentic();
    test_half();
    test_not_a_cadence();
    test_cadential_progression_rule();
    test_subverted();
    test_strength_order();
    test_invalid_cadence();
    printf("test_harmony_cadence_rules: ok\n");
    return 0;
}
