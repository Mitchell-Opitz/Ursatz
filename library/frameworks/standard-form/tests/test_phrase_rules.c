#include <assert.h>
#include <stdio.h>

#include "phrase_rules.h"

static void test_phrase_goal(void) {
    assert(standard_form_cadence_is_phrase_goal(CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(standard_form_cadence_is_phrase_goal(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(standard_form_cadence_is_phrase_goal(CADENCE_TYPE_HALF));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_SUBVERTED));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_PLAGAL));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_DECEPTIVE));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_UNKNOWN));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_COUNT));
}

int main(void) {
    test_phrase_goal();
    printf("test_phrase_rules (standard-form): ok\n");
    return 0;
}
