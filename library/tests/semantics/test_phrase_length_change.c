#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/phrase_length_change.h"

static void test_validity(void) {
    for (int i = 0; i < PHRASE_LENGTH_CHANGE_COUNT; i++) {
        assert(phrase_length_change_valid((PhraseLengthChange)i));
    }
    assert(!phrase_length_change_valid(PHRASE_LENGTH_CHANGE_COUNT));
    assert(!phrase_length_change_valid((PhraseLengthChange)-1));
}

static void test_names(void) {
    assert(strcmp(phrase_length_change_name(PHRASE_LENGTH_CHANGE_EXPANSION), "expansion") == 0);
    assert(strcmp(phrase_length_change_name(PHRASE_LENGTH_CHANGE_CONTRACTION), "contraction") == 0);
    assert(phrase_length_change_name(PHRASE_LENGTH_CHANGE_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_phrase_length_change: ok\n");
    return 0;
}
