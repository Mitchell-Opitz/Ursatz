#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/counterpoint/motion_type.h"

static void test_same_direction(void) {
    assert(motion_type_is_same_direction(MOTION_TYPE_SIMILAR));
    assert(motion_type_is_same_direction(MOTION_TYPE_PARALLEL));
    assert(!motion_type_is_same_direction(MOTION_TYPE_CONTRARY));
    assert(!motion_type_is_same_direction(MOTION_TYPE_OBLIQUE));
    assert(!motion_type_is_same_direction(MOTION_TYPE_STATIC));
    assert(!motion_type_is_same_direction(MOTION_TYPE_COUNT));
}

int main(void) {
    test_same_direction();
    printf("test_motion_type: ok\n");
    return 0;
}
