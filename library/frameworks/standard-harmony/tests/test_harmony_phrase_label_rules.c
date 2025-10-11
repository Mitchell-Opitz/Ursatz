#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_phrase_label_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "labels", HARMONY_LABELS_RULE_COUNT);
#define harmony_labels_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_labels_constraint, harmony_labels_rule_holds, HarmonyHeardPhrase, which, subject)

#define LAB(name) HARMONY_LABELS_##name

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define IAC CADENCE_TYPE_IMPERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

#define I CH(1)
#define IV CH(4)
#define V CH(5)
#define VI CH(6)

static HarmonyHeardPhrase heard(HarmonyMode mode, const HarmonyChord *chords, size_t count, CadenceType cadence) {
    return (HarmonyHeardPhrase){{mode, chords, count}, cadence};
}

static bool holds(HarmonyLabelsRule rule, const HarmonyHeardPhrase *h) {
    return harmony_labels_rule_holds(rule, h);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_labels_rule, harmony_labels_constraint, HARMONY_LABELS_RULE_COUNT);
    for (int i = 0; i < HARMONY_LABELS_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_labels_constraint((HarmonyLabelsRule)i));
        assert(type == (i == LAB(ENDS_AS_CADENCE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
    assert(strcmp(harmony_labels_rule_name(LAB(ENDS_AS_CADENCE)), "ends-as-cadence") == 0);
    assert(strcmp(harmony_labels_rule_name(LAB(STARTS_ON_TONIC)), "starts-on-tonic") == 0);
    assert(strcmp(harmony_labels_rule_name(LAB(HAS_STRONG_PREDOMINANT)), "has-strong-predominant") == 0);
    assert(harmony_labels_rule_name(LAB(RULE_COUNT)) == NULL);
}

static void test_the_full_phrase_model_holds_every_rule(void) {
    const HarmonyChord authentic[] = {I, IV, V, I};
    const HarmonyChord half[] = {I, IV, V};
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyHeardPhrase phrases[] = {
            heard(m, authentic, COUNT_OF(authentic), PAC),
            heard(m, authentic, COUNT_OF(authentic), IAC),
            heard(m, half, COUNT_OF(half), HC),
        };
        for (size_t i = 0; i < COUNT_OF(phrases); i++) {
            assert(harmony_heard_phrase_valid(&phrases[i]));
            for (int rule = 0; rule < HARMONY_LABELS_RULE_COUNT; rule++) {
                assert(holds((HarmonyLabelsRule)rule, &phrases[i]));
            }
            unsigned hard = 99, soft = 99;
            assert(harmony_labels_check(&phrases[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_the_phrase_must_end_as_the_cadence_says(void) {
    const HarmonyChord plagal[] = {I, IV, I};
    const HarmonyChord ends_on_v[] = {I, IV, V};
    HarmonyHeardPhrase plagal_heard = heard(HARMONY_MODE_MAJOR, plagal, COUNT_OF(plagal), PAC);
    HarmonyHeardPhrase half_on_tonic = heard(HARMONY_MODE_MAJOR, plagal, COUNT_OF(plagal), HC);
    HarmonyHeardPhrase authentic_on_v = heard(HARMONY_MODE_MAJOR, ends_on_v, COUNT_OF(ends_on_v), IAC);
    HarmonyHeardPhrase subverted = heard(HARMONY_MODE_MAJOR, ends_on_v, COUNT_OF(ends_on_v), CADENCE_TYPE_SUBVERTED);
    assert(!holds(LAB(ENDS_AS_CADENCE), &plagal_heard));
    assert(!holds(LAB(ENDS_AS_CADENCE), &half_on_tonic));
    assert(!holds(LAB(ENDS_AS_CADENCE), &authentic_on_v));
    assert(!holds(LAB(ENDS_AS_CADENCE), &subverted));

    /* It is a hard rule; with no cadence there is nothing to label either. */
    unsigned hard = 0, soft = 0;
    assert(harmony_labels_check(&plagal_heard, &hard, &soft));
    assert(hard == (1u << LAB(ENDS_AS_CADENCE)));
    assert(soft == ((1u << LAB(STARTS_ON_TONIC)) | (1u << LAB(HAS_STRONG_PREDOMINANT))));
}

static void test_a_phrase_usually_starts_on_tonic(void) {
    const HarmonyChord delayed[] = {IV, V, I};
    const HarmonyChord from_v[] = {V, I};
    const HarmonyChord prolonged[] = {I, CH7I(5, 1), I, IV, V, I};
    HarmonyHeardPhrase delayed_heard = heard(HARMONY_MODE_MAJOR, delayed, COUNT_OF(delayed), PAC);
    HarmonyHeardPhrase from_v_heard = heard(HARMONY_MODE_MAJOR, from_v, COUNT_OF(from_v), PAC);
    HarmonyHeardPhrase prolonged_heard = heard(HARMONY_MODE_MAJOR, prolonged, COUNT_OF(prolonged), PAC);
    assert(!holds(LAB(STARTS_ON_TONIC), &delayed_heard));
    assert(!holds(LAB(STARTS_ON_TONIC), &from_v_heard));
    assert(holds(LAB(STARTS_ON_TONIC), &prolonged_heard));

    /* A delayed tonic is rare, not wrong: nothing hard breaks. */
    unsigned hard = 99, soft = 0;
    assert(harmony_labels_check(&delayed_heard, &hard, &soft));
    assert(hard == 0 && soft == (1u << LAB(STARTS_ON_TONIC)));
}

static void test_a_strong_predominant_usually_comes_before_the_dominant(void) {
    const HarmonyChord none[] = {I, V, I};
    const HarmonyChord weak[] = {I, VI, V, I};
    const HarmonyChord with_six_four[] = {I, IV, CADENTIAL_CHORD, V, I};
    HarmonyHeardPhrase none_heard = heard(HARMONY_MODE_MAJOR, none, COUNT_OF(none), PAC);
    HarmonyHeardPhrase weak_heard = heard(HARMONY_MODE_MAJOR, weak, COUNT_OF(weak), PAC);
    HarmonyHeardPhrase six_four_heard = heard(HARMONY_MODE_MAJOR, with_six_four, COUNT_OF(with_six_four), PAC);
    assert(!holds(LAB(HAS_STRONG_PREDOMINANT), &none_heard));
    assert(!holds(LAB(HAS_STRONG_PREDOMINANT), &weak_heard));
    assert(holds(LAB(HAS_STRONG_PREDOMINANT), &six_four_heard));

    unsigned hard = 99, soft = 0;
    assert(harmony_labels_check(&none_heard, &hard, &soft));
    assert(hard == 0 && soft == (1u << LAB(HAS_STRONG_PREDOMINANT)));
}

static void test_invalid_heard_phrase(void) {
    const HarmonyChord ok[] = {I, IV, V, I};
    const HarmonyChord bad_chord[] = {I, CH(9), V, I};
    HarmonyHeardPhrase bad_cadence = heard(HARMONY_MODE_MAJOR, ok, COUNT_OF(ok), CADENCE_TYPE_COUNT);
    HarmonyHeardPhrase bad_phrase = heard(HARMONY_MODE_MAJOR, bad_chord, COUNT_OF(bad_chord), PAC);
    HarmonyHeardPhrase bad_mode = heard(HARMONY_MODE_COUNT, ok, COUNT_OF(ok), PAC);
    HarmonyHeardPhrase *bad[] = {&bad_cadence, &bad_phrase, &bad_mode};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_heard_phrase_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_LABELS_RULE_COUNT; rule++) {
            assert(!holds((HarmonyLabelsRule)rule, bad[i]));
        }
        assert(!harmony_labels_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_heard_phrase_valid(NULL));
    HarmonyHeardPhrase good = heard(HARMONY_MODE_MAJOR, ok, COUNT_OF(ok), PAC);
    assert(!holds(LAB(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_the_full_phrase_model_holds_every_rule();
    test_the_phrase_must_end_as_the_cadence_says();
    test_a_phrase_usually_starts_on_tonic();
    test_a_strong_predominant_usually_comes_before_the_dominant();
    test_invalid_heard_phrase();
    printf("test_harmony_phrase_label_rules: ok\n");
    return 0;
}
