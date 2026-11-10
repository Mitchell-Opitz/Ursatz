#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "phrase_ending.h"

static void test_validity(void) {
    for (int i = 0; i < PHRASE_ENDING_COUNT; i++) {
        assert(phrase_ending_valid((PhraseEnding)i));
    }
    assert(!phrase_ending_valid(PHRASE_ENDING_COUNT));
    assert(!phrase_ending_valid((PhraseEnding)-1));
}

static void test_names(void) {
    assert(strcmp(phrase_ending_name(PHRASE_ENDING_CONSEQUENT), "consequent") == 0);
    assert(strcmp(phrase_ending_name(PHRASE_ENDING_CONTINUATION), "continuation") == 0);
    assert(strcmp(phrase_ending_name(PHRASE_ENDING_CADENTIAL), "cadential") == 0);
    assert(phrase_ending_name(PHRASE_ENDING_COUNT) == NULL);
}

static void test_from_name(void) {
    for (int i = 0; i < PHRASE_ENDING_COUNT; i++) {
        PhraseEnding value = (PhraseEnding)-1;
        assert(phrase_ending_from_name(phrase_ending_name((PhraseEnding)i), &value));
        assert(value == (PhraseEnding)i);
    }
    PhraseEnding untouched = (PhraseEnding)-1;
    assert(!phrase_ending_from_name("no-such-name", &untouched));
    assert(!phrase_ending_from_name(NULL, &untouched));
    assert(untouched == (PhraseEnding)-1);
}

int main(void) {
    test_validity();
    test_names();
    test_from_name();
    printf("test_phrase_ending: ok\n");
    return 0;
}
