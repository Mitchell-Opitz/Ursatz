#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sequence_kind.h"

static void test_validity(void) {
    for (int i = 0; i < SEQUENCE_KIND_COUNT; i++) {
        assert(sequence_kind_valid((SequenceKind)i));
    }
    assert(!sequence_kind_valid(SEQUENCE_KIND_COUNT));
    assert(!sequence_kind_valid((SequenceKind)-1));
}

static void test_names(void) {
    assert(strcmp(sequence_kind_name(SEQUENCE_KIND_PARALLEL_CHORDS), "parallel-chords") == 0);
    assert(strcmp(sequence_kind_name(SEQUENCE_KIND_FIVE_SIX), "five-six") == 0);
    assert(strcmp(sequence_kind_name(SEQUENCE_KIND_SEVEN_SIX), "seven-six") == 0);
    assert(strcmp(sequence_kind_name(SEQUENCE_KIND_CYCLE_OF_FIFTHS), "cycle-of-fifths") == 0);
    assert(strcmp(sequence_kind_name(SEQUENCE_KIND_TWO_THREE), "two-three") == 0);
    assert(sequence_kind_name(SEQUENCE_KIND_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    printf("test_sequence_kind: ok\n");
    return 0;
}
