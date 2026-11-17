#include <assert.h>
#include <stdio.h>

#include "theory/form/combination_rules.h"

/* A repeated phrase ends both times with the same cadence, whatever it is. */
static void test_repeated_phrase(void) {
    assert(standard_form_is_repeated_phrase(true, CADENCE_TYPE_HALF, CADENCE_TYPE_HALF));
    assert(standard_form_is_repeated_phrase(true, CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(standard_form_is_repeated_phrase(true, CADENCE_TYPE_SUBVERTED, CADENCE_TYPE_SUBVERTED));
    assert(!standard_form_is_repeated_phrase(true, CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(!standard_form_is_repeated_phrase(false, CADENCE_TYPE_HALF, CADENCE_TYPE_HALF));
    assert(!standard_form_is_repeated_phrase(true, CADENCE_TYPE_COUNT, CADENCE_TYPE_COUNT));
}

static void test_compound_period(void) {
    assert(standard_form_is_compound_period(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC,
                                            PHRASE_LEVEL_FORM_SENTENCE, PHRASE_LEVEL_FORM_SENTENCE));
    assert(!standard_form_is_compound_period(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC,
                                             PHRASE_LEVEL_FORM_SENTENCE, PHRASE_LEVEL_FORM_UNIQUE));
    assert(!standard_form_is_compound_period(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC,
                                             PHRASE_LEVEL_FORM_UNIQUE, PHRASE_LEVEL_FORM_SENTENCE));
    /* two sentences with the same cadence are not a period */
    assert(!standard_form_is_compound_period(CADENCE_TYPE_HALF, CADENCE_TYPE_HALF, PHRASE_LEVEL_FORM_SENTENCE,
                                             PHRASE_LEVEL_FORM_SENTENCE));
}

static void test_classify_phrase_pair(void) {
    PhraseLevelForm form = PHRASE_LEVEL_FORM_UNIQUE;

    assert(standard_form_classify_phrase_pair(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC, false,
                                              PHRASE_LEVEL_FORM_UNIQUE, PHRASE_LEVEL_FORM_UNIQUE, &form));
    assert(form == PHRASE_LEVEL_FORM_PERIOD);

    assert(standard_form_classify_phrase_pair(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC, false,
                                              PHRASE_LEVEL_FORM_SENTENCE, PHRASE_LEVEL_FORM_SENTENCE, &form));
    assert(form == PHRASE_LEVEL_FORM_COMPOUND_PERIOD);

    assert(standard_form_classify_phrase_pair(CADENCE_TYPE_HALF, CADENCE_TYPE_HALF, true, PHRASE_LEVEL_FORM_UNIQUE,
                                              PHRASE_LEVEL_FORM_UNIQUE, &form));
    assert(form == PHRASE_LEVEL_FORM_REPEATED_PHRASE);

    /* a written-out repeat that ends stronger is a period, not a repeated phrase */
    assert(standard_form_classify_phrase_pair(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC, true,
                                              PHRASE_LEVEL_FORM_UNIQUE, PHRASE_LEVEL_FORM_UNIQUE, &form));
    assert(form == PHRASE_LEVEL_FORM_PERIOD);

    /* neither: a weaker second cadence, or the same cadence without a repeat */
    form = PHRASE_LEVEL_FORM_UNIQUE;
    assert(!standard_form_classify_phrase_pair(CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_HALF, false,
                                               PHRASE_LEVEL_FORM_UNIQUE, PHRASE_LEVEL_FORM_UNIQUE, &form));
    assert(!standard_form_classify_phrase_pair(CADENCE_TYPE_HALF, CADENCE_TYPE_HALF, false, PHRASE_LEVEL_FORM_UNIQUE,
                                               PHRASE_LEVEL_FORM_UNIQUE, &form));
    assert(form == PHRASE_LEVEL_FORM_UNIQUE);
}

int main(void) {
    test_repeated_phrase();
    test_compound_period();
    test_classify_phrase_pair();
    printf("test_combination_rules (standard-form): ok\n");
    return 0;
}
