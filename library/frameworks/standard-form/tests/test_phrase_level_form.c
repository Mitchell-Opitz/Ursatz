#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "phrase_level_form.h"

static void test_validity(void) {
    for (int i = 0; i < PHRASE_LEVEL_FORM_COUNT; i++) {
        assert(phrase_level_form_valid((PhraseLevelForm)i));
    }
    assert(!phrase_level_form_valid(PHRASE_LEVEL_FORM_COUNT));
    assert(!phrase_level_form_valid((PhraseLevelForm)-1));
}

static void test_names(void) {
    assert(strcmp(phrase_level_form_name(PHRASE_LEVEL_FORM_SENTENCE), "sentence") == 0);
    assert(strcmp(phrase_level_form_name(PHRASE_LEVEL_FORM_PERIOD), "period") == 0);
    assert(strcmp(phrase_level_form_name(PHRASE_LEVEL_FORM_REPEATED_PHRASE), "repeated-phrase") == 0);
    assert(strcmp(phrase_level_form_name(PHRASE_LEVEL_FORM_COMPOUND_PERIOD), "compound-period") == 0);
    assert(strcmp(phrase_level_form_name(PHRASE_LEVEL_FORM_HYBRID), "hybrid") == 0);
    assert(strcmp(phrase_level_form_name(PHRASE_LEVEL_FORM_UNIQUE), "unique") == 0);
    assert(phrase_level_form_name(PHRASE_LEVEL_FORM_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_phrase_level_form: ok\n");
    return 0;
}
