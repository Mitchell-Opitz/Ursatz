#include "motion_type.h"

#include <stddef.h>

bool motion_type_is_same_direction(MotionType type) {
    return type == MOTION_TYPE_SIMILAR || type == MOTION_TYPE_PARALLEL;
}
