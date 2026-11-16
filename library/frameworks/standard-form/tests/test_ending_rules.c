#include <assert.h>
#include <stdio.h>

#include "ending_rules.h"

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
    test_classify();
    printf("test_ending_rules (standard-form): ok\n");
    return 0;
}
