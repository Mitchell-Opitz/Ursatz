#include <assert.h>
#include <stdio.h>

#include "ending_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_ending_rule, standard_form_ending_constraint,
                                    STANDARD_FORM_ENDING_RULE_COUNT);
}

/* Example 5: consequent and cadential take PAC/IAC, never HC; a continuation also takes HC. */
static void test_cadence(void) {
    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CONSEQUENT, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CONSEQUENT, CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(!standard_form_ending_cadence_allowed(PHRASE_ENDING_CONSEQUENT, CADENCE_TYPE_HALF));

    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CADENTIAL, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CADENTIAL, CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(!standard_form_ending_cadence_allowed(PHRASE_ENDING_CADENTIAL, CADENCE_TYPE_HALF));

    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CONTINUATION, CADENCE_TYPE_HALF));
    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CONTINUATION, CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(standard_form_ending_cadence_allowed(PHRASE_ENDING_CONTINUATION, CADENCE_TYPE_PERFECT_AUTHENTIC));

    assert(!standard_form_ending_cadence_allowed(PHRASE_ENDING_CONTINUATION, CADENCE_TYPE_SUBVERTED));
    assert(!standard_form_ending_cadence_allowed(PHRASE_ENDING_CONSEQUENT, CADENCE_TYPE_COUNT));
    assert(!standard_form_ending_cadence_allowed(PHRASE_ENDING_COUNT, CADENCE_TYPE_PERFECT_AUTHENTIC));
}

static void test_division(void) {
    size_t one_one_two[3] = {1, 1, 2};
    size_t two_two[2] = {2, 2};
    size_t whole[1] = {4};
    size_t one_three[2] = {1, 3};

    assert(standard_form_ending_division_usual(PHRASE_ENDING_CONTINUATION, one_one_two, 3));
    assert(standard_form_ending_division_usual(PHRASE_ENDING_CONTINUATION, two_two, 2));
    assert(!standard_form_ending_division_usual(PHRASE_ENDING_CONTINUATION, one_three, 2));
    assert(!standard_form_ending_division_usual(PHRASE_ENDING_CONTINUATION, whole, 1));

    assert(standard_form_ending_division_usual(PHRASE_ENDING_CADENTIAL, two_two, 2));
    assert(standard_form_ending_division_usual(PHRASE_ENDING_CADENTIAL, whole, 1));
    assert(!standard_form_ending_division_usual(PHRASE_ENDING_CADENTIAL, one_one_two, 3));

    /* a consequent is a basic idea then a contrasting idea, whatever their lengths */
    assert(standard_form_ending_division_usual(PHRASE_ENDING_CONSEQUENT, two_two, 2));
    assert(standard_form_ending_division_usual(PHRASE_ENDING_CONSEQUENT, one_three, 2));
    assert(!standard_form_ending_division_usual(PHRASE_ENDING_CONSEQUENT, whole, 1));

    assert(!standard_form_ending_division_usual(PHRASE_ENDING_COUNT, two_two, 2));
    assert(!standard_form_ending_division_usual(PHRASE_ENDING_CONTINUATION, NULL, 0));
}

static void test_fragmentation(void) {
    assert(standard_form_ending_allows_fragmentation(PHRASE_ENDING_CONSEQUENT));
    assert(standard_form_ending_allows_fragmentation(PHRASE_ENDING_CONTINUATION));
    assert(!standard_form_ending_allows_fragmentation(PHRASE_ENDING_CADENTIAL));
    assert(!standard_form_ending_allows_fragmentation(PHRASE_ENDING_COUNT));
}

static void test_stability(void) {
    StandardFormStability stability = STANDARD_FORM_STABILITY_COUNT;

    assert(standard_form_ending_start_stability(PHRASE_ENDING_CONSEQUENT, &stability));
    assert(stability == STANDARD_FORM_STABILITY_STABLE);
    assert(standard_form_ending_start_stability(PHRASE_ENDING_CONTINUATION, &stability));
    assert(stability == STANDARD_FORM_STABILITY_UNSTABLE);

    /* a cadential ending is not characterized */
    stability = STANDARD_FORM_STABILITY_COUNT;
    assert(!standard_form_ending_start_stability(PHRASE_ENDING_CADENTIAL, &stability));
    assert(!standard_form_ending_start_stability(PHRASE_ENDING_COUNT, &stability));
    assert(stability == STANDARD_FORM_STABILITY_COUNT);
}

static void test_classify(void) {
    PhraseEnding ending = PHRASE_ENDING_CONSEQUENT;

    assert(standard_form_classify_ending(CADENCE_TYPE_PERFECT_AUTHENTIC, true, false, false, &ending));
    assert(ending == PHRASE_ENDING_CADENTIAL);
    assert(standard_form_classify_ending(CADENCE_TYPE_IMPERFECT_AUTHENTIC, true, true, false, &ending));
    assert(ending == PHRASE_ENDING_CADENTIAL); /* the bass settles a combination that fits two */

    assert(standard_form_classify_ending(CADENCE_TYPE_PERFECT_AUTHENTIC, false, true, false, &ending));
    assert(ending == PHRASE_ENDING_CONSEQUENT);

    assert(standard_form_classify_ending(CADENCE_TYPE_HALF, false, false, true, &ending));
    assert(ending == PHRASE_ENDING_CONTINUATION);
    assert(standard_form_classify_ending(CADENCE_TYPE_PERFECT_AUTHENTIC, false, false, true, &ending));
    assert(ending == PHRASE_ENDING_CONTINUATION);
    /* a cadential bass with fragmentation, or with a half cadence, is not cadential */
    assert(standard_form_classify_ending(CADENCE_TYPE_PERFECT_AUTHENTIC, true, false, true, &ending));
    assert(ending == PHRASE_ENDING_CONTINUATION);
    assert(standard_form_classify_ending(CADENCE_TYPE_HALF, true, true, false, &ending));
    assert(ending == PHRASE_ENDING_CONTINUATION);

    ending = PHRASE_ENDING_CADENTIAL;
    assert(!standard_form_classify_ending(CADENCE_TYPE_SUBVERTED, true, true, false, &ending));
    assert(!standard_form_classify_ending(CADENCE_TYPE_COUNT, false, false, false, &ending));
    assert(ending == PHRASE_ENDING_CADENTIAL);
}

int main(void) {
    test_rules_reference_framework();
    test_cadence();
    test_division();
    test_fragmentation();
    test_stability();
    test_classify();
    printf("test_ending_rules (standard-form): ok\n");
    return 0;
}
