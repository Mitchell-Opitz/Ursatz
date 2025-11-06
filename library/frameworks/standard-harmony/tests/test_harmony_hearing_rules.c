#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_hearing_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "hearing", HARMONY_HEARING_RULE_COUNT);
#define harmony_hearing_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_hearing_constraint, harmony_hearing_preference, harmony_hearing_rule_holds, HarmonyCadenceHearing, which, subject)

#define HEARING(name) HARMONY_HEARING_##name

/* A true cadence: V to I with do in the soprano, a beginning after it, a phrase
   of four measures at a moderate tempo. */
static HarmonyCadenceHearing true_cadence(void) {
    HarmonyCadence candidate = {CH(5), CH(1), 1, false};
    return (HarmonyCadenceHearing){candidate, true, 4, false};
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_hearing_rule, harmony_hearing_constraint, HARMONY_HEARING_RULE_COUNT);
    for (int i = 0; i < HARMONY_HEARING_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_hearing_constraint((HarmonyHearingRule)i));
        assert(type == (i <= HEARING(NOT_SUBVERTED) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(harmony_hearing_rule_name(HEARING(POTENTIAL_CADENCE)), "potential-cadence") == 0);
    assert(strcmp(harmony_hearing_rule_name(HEARING(BEGINNING_FOLLOWS)), "beginning-follows") == 0);
    assert(strcmp(harmony_hearing_rule_name(HEARING(NOT_SUBVERTED)), "not-subverted") == 0);
    assert(strcmp(harmony_hearing_rule_name(HEARING(PHRASE_LENGTH)), "phrase-length") == 0);
    assert(harmony_hearing_rule_name(HEARING(RULE_COUNT)) == NULL);
}

static void test_true_cadence_holds(void) {
    HarmonyCadenceHearing h = true_cadence();
    for (int i = 0; i < HARMONY_HEARING_RULE_COUNT; i++) {
        assert(harmony_hearing_rule_holds((HarmonyHearingRule)i, &h));
    }
    unsigned hard = 99, soft = 99;
    assert(harmony_hearing_check(&h, &hard, &soft));
    assert(hard == 0 && soft == 0);

    /* A half cadence is a true cadence too. */
    h.candidate = (HarmonyCadence){CH(4), CH(5), 7, false};
    assert(harmony_hearing_check(&h, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_potential_cadence(void) {
    HarmonyCadenceHearing h = true_cadence();
    h.candidate = (HarmonyCadence){CH(4), CH(1), 1, false}; /* plagal: an ending, not a cadence */
    assert(!harmony_hearing_rule_holds(HEARING(POTENTIAL_CADENCE), &h));
    h.candidate = (HarmonyCadence){CH(5), CH(6), 1, false}; /* deceptive */
    assert(!harmony_hearing_rule_holds(HEARING(POTENTIAL_CADENCE), &h));
}

static void test_beginning_follows(void) {
    HarmonyCadenceHearing h = true_cadence();
    h.beginning_follows = false;
    assert(!harmony_hearing_rule_holds(HEARING(BEGINNING_FOLLOWS), &h));
    unsigned hard = 0, soft = 0;
    assert(harmony_hearing_check(&h, &hard, &soft));
    assert(hard == (1u << HEARING(BEGINNING_FOLLOWS)) && soft == 0);
}

static void test_subverted_is_not_a_true_cadence(void) {
    HarmonyCadenceHearing h = true_cadence();
    h.candidate.subverted = true;
    assert(!harmony_hearing_rule_holds(HEARING(NOT_SUBVERTED), &h));
    /* The harmonies were still a potential cadence. */
    assert(harmony_hearing_rule_holds(HEARING(POTENTIAL_CADENCE), &h));
}

static void test_phrase_length(void) {
    HarmonyCadenceHearing h = true_cadence();
    h.phrase_measures = 2;
    assert(!harmony_hearing_rule_holds(HEARING(PHRASE_LENGTH), &h));
    h.slow_tempo = true;
    assert(harmony_hearing_rule_holds(HEARING(PHRASE_LENGTH), &h));
    h.slow_tempo = false;
    h.phrase_measures = 3;
    assert(harmony_hearing_rule_holds(HEARING(PHRASE_LENGTH), &h));
    h.phrase_measures = 8;
    assert(harmony_hearing_rule_holds(HEARING(PHRASE_LENGTH), &h));

    /* A two-measure phrase is only advice against: soft, not hard. */
    h.phrase_measures = 2;
    unsigned hard = 99, soft = 99;
    assert(harmony_hearing_check(&h, &hard, &soft));
    assert(hard == 0 && soft == (1u << HEARING(PHRASE_LENGTH)));
}

static void test_invalid_hearing(void) {
    HarmonyCadenceHearing bad_candidate = true_cadence();
    bad_candidate.candidate.soprano_degree = 0;
    HarmonyCadenceHearing no_measures = true_cadence();
    no_measures.phrase_measures = 0;
    HarmonyCadenceHearing *bad[] = {&bad_candidate, &no_measures};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_hearing_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_HEARING_RULE_COUNT; rule++) {
            assert(!harmony_hearing_rule_holds((HarmonyHearingRule)rule, bad[i]));
        }
        assert(!harmony_hearing_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_hearing_valid(NULL));
    HarmonyCadenceHearing ok = true_cadence();
    assert(!harmony_hearing_rule_holds(HEARING(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_true_cadence_holds();
    test_potential_cadence();
    test_beginning_follows();
    test_subverted_is_not_a_true_cadence();
    test_phrase_length();
    test_invalid_hearing();
    printf("test_harmony_hearing_rules: ok\n");
    return 0;
}
