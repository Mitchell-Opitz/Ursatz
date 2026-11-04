#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/motive_occurrence_kind.h"

static void test_validity(void) {
    for (int i = 0; i < MOTIVE_OCCURRENCE_KIND_COUNT; i++) {
        assert(motive_occurrence_kind_valid((MotiveOccurrenceKind)i));
    }
    assert(!motive_occurrence_kind_valid(MOTIVE_OCCURRENCE_KIND_COUNT));
    assert(!motive_occurrence_kind_valid((MotiveOccurrenceKind)-1));
}

static void test_values_keep_their_order(void) {
    assert(MOTIVE_OCCURRENCE_EXACT == 0 && MOTIVE_OCCURRENCE_REAL == 1 && MOTIVE_OCCURRENCE_TONAL == 2);
    assert(MOTIVE_OCCURRENCE_INVERTED == 3 && MOTIVE_OCCURRENCE_RETROGRADE == 4 && MOTIVE_OCCURRENCE_AUGMENTED == 5);
    assert(MOTIVE_OCCURRENCE_DIMINISHED == 6 && MOTIVE_OCCURRENCE_FRAGMENTED == 7 && MOTIVE_OCCURRENCE_VARIED == 8);
    assert(MOTIVE_OCCURRENCE_KIND_COUNT == 9);
}

static void test_names(void) {
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_EXACT), "exact") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_REAL), "real-transposed") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_TONAL), "tonal-transposed") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_INVERTED), "inverted") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_RETROGRADE), "retrograde") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_AUGMENTED), "augmented") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_DIMINISHED), "diminished") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_FRAGMENTED), "fragmented") == 0);
    assert(strcmp(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_VARIED), "varied") == 0);
    assert(motive_occurrence_kind_name(MOTIVE_OCCURRENCE_KIND_COUNT) == NULL);
    assert(motive_occurrence_kind_name((MotiveOccurrenceKind)-1) == NULL);
}

static void test_ranks(void) {
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_EXACT) == 0);
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_REAL) == 1 && motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_TONAL) == 1);
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_INVERTED) == 2 && motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_RETROGRADE) == 2);
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_AUGMENTED) == 3 && motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_DIMINISHED) == 3);
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_FRAGMENTED) == 4);
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_VARIED) == 5);
    assert(motive_occurrence_kind_rank(MOTIVE_OCCURRENCE_KIND_COUNT) == -1);
    assert(motive_occurrence_kind_rank((MotiveOccurrenceKind)-1) == -1);
}

int main(void) {
    test_validity();
    test_values_keep_their_order();
    test_names();
    test_ranks();
    printf("test_motive_occurrence_kind: ok\n");
    return 0;
}
