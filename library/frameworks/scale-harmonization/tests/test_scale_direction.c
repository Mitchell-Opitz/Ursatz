#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scale_direction.h"

static void test_validity(void) {
    for (int i = 0; i < SCALE_DIRECTION_COUNT; i++) {
        assert(scale_direction_valid((ScaleDirection)i));
    }
    assert(!scale_direction_valid(SCALE_DIRECTION_COUNT));
    assert(!scale_direction_valid((ScaleDirection)-1));
}

static void test_names(void) {
    assert(strcmp(scale_direction_name(SCALE_DIRECTION_ASCENDING), "ascending") == 0);
    assert(strcmp(scale_direction_name(SCALE_DIRECTION_DESCENDING), "descending") == 0);
    assert(scale_direction_name(SCALE_DIRECTION_COUNT) == NULL);
}

static void test_opposite(void) {
    assert(scale_direction_opposite(SCALE_DIRECTION_ASCENDING) == SCALE_DIRECTION_DESCENDING);
    assert(scale_direction_opposite(SCALE_DIRECTION_DESCENDING) == SCALE_DIRECTION_ASCENDING);
    assert(scale_direction_opposite(SCALE_DIRECTION_COUNT) == SCALE_DIRECTION_COUNT);
}

int main(void) {
    test_validity();
    test_names();
    test_opposite();
    printf("test_scale_direction: ok\n");
    return 0;
}
