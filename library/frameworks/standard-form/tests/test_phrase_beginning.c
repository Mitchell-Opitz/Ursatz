#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "phrase_beginning.h"

static void test_validity(void) {
    for (int i = 0; i < PHRASE_BEGINNING_COUNT; i++) {
        assert(phrase_beginning_valid((PhraseBeginning)i));
    }
    assert(!phrase_beginning_valid(PHRASE_BEGINNING_COUNT));
    assert(!phrase_beginning_valid((PhraseBeginning)-1));
}

static void test_names(void) {
    assert(strcmp(phrase_beginning_name(PHRASE_BEGINNING_ANTECEDENT), "antecedent") == 0);
    assert(strcmp(phrase_beginning_name(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA), "compound-basic-idea") == 0);
    assert(strcmp(phrase_beginning_name(PHRASE_BEGINNING_PRESENTATION), "presentation") == 0);
    assert(phrase_beginning_name(PHRASE_BEGINNING_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_phrase_beginning: ok\n");
    return 0;
}
