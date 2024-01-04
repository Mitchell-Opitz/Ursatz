#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "motion_type.h"

static void test_validity(void) {
    for (int i = 0; i < MOTION_TYPE_COUNT; i++) {
        assert(motion_type_valid((MotionType)i));
    }
    assert(!motion_type_valid(MOTION_TYPE_COUNT));
    assert(!motion_type_valid((MotionType)-1));
}

static void test_names(void) {
    assert(strcmp(motion_type_name(MOTION_TYPE_CONTRARY), "contrary") == 0);
    assert(strcmp(motion_type_name(MOTION_TYPE_SIMILAR), "similar") == 0);
    assert(strcmp(motion_type_name(MOTION_TYPE_PARALLEL), "parallel") == 0);
    assert(strcmp(motion_type_name(MOTION_TYPE_OBLIQUE), "oblique") == 0);
    assert(strcmp(motion_type_name(MOTION_TYPE_STATIC), "static") == 0);
    assert(motion_type_name(MOTION_TYPE_COUNT) == NULL);
}

static void test_same_direction(void) {
    assert(motion_type_is_same_direction(MOTION_TYPE_SIMILAR));
    assert(motion_type_is_same_direction(MOTION_TYPE_PARALLEL));
    assert(!motion_type_is_same_direction(MOTION_TYPE_CONTRARY));
    assert(!motion_type_is_same_direction(MOTION_TYPE_OBLIQUE));
    assert(!motion_type_is_same_direction(MOTION_TYPE_STATIC));
    assert(!motion_type_is_same_direction(MOTION_TYPE_COUNT));
}

int main(void) {
    test_validity();
    test_names();
    test_same_direction();
    printf("test_motion_type: ok\n");
    return 0;
}
