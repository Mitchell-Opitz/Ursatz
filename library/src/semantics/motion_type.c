#include "semantics/motion_type.h"

#include <stddef.h>

static const char *const NAMES[MOTION_TYPE_COUNT] = {"contrary", "similar", "parallel", "oblique", "static"};

bool motion_type_valid(MotionType type) {
    return type >= 0 && type < MOTION_TYPE_COUNT;
}

const char *motion_type_name(MotionType type) {
    return motion_type_valid(type) ? NAMES[type] : NULL;
}

bool motion_type_is_same_direction(MotionType type) {
    return type == MOTION_TYPE_SIMILAR || type == MOTION_TYPE_PARALLEL;
}
