#include "model/time/tuplet.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static Tuplet *make(unsigned actual, unsigned normal) {
    Tuplet *tuplet = NULL;
    assert(tuplet_create(actual, normal, &tuplet) == TUPLET_OK);
    return tuplet;
}

static void test_create_rejects_invalid(void) {
    Tuplet *tuplet = NULL;
    assert(tuplet_create(0, 2, &tuplet) == TUPLET_ERR_ACTUAL_OUT_OF_RANGE);
    assert(tuplet_create(1, 2, &tuplet) == TUPLET_ERR_ACTUAL_OUT_OF_RANGE);
    assert(tuplet_create(TUPLET_MAX_COUNT + 1, 2, &tuplet) == TUPLET_ERR_ACTUAL_OUT_OF_RANGE);
    assert(tuplet_create(3, 0, &tuplet) == TUPLET_ERR_NORMAL_OUT_OF_RANGE);
    assert(tuplet_create(3, TUPLET_MAX_COUNT + 1, &tuplet) == TUPLET_ERR_NORMAL_OUT_OF_RANGE);
    assert(tuplet_create(3, 3, &tuplet) == TUPLET_ERR_EQUAL_COUNTS);
    assert(tuplet_create(2, 2, &tuplet) == TUPLET_ERR_EQUAL_COUNTS);
    assert(tuplet == NULL);
}

static void test_accessors_and_bounds(void) {
    Tuplet *triplet = make(3, 2);
    assert(tuplet_actual(triplet) == 3 && tuplet_normal(triplet) == 2);
    tuplet_destroy(triplet);

    Tuplet *duplet = make(2, 3);
    assert(tuplet_actual(duplet) == 2 && tuplet_normal(duplet) == 3);
    tuplet_destroy(duplet);

    /* normal may be larger or smaller than actual; both bounds accepted */
    Tuplet *widest = make(TUPLET_MAX_COUNT, 1);
    tuplet_destroy(widest);
    Tuplet *narrowest = make(2, TUPLET_MAX_COUNT);
    tuplet_destroy(narrowest);
}

int main(void) {
    test_create_rejects_invalid();
    test_accessors_and_bounds();
    printf("test_tuplet: ok\n");
    return 0;
}
