#include "scale_direction.h"

#include <stddef.h>

static const char *const NAMES[SCALE_DIRECTION_COUNT] = {"ascending", "descending"};

bool scale_direction_valid(ScaleDirection direction) {
    return direction >= 0 && direction < SCALE_DIRECTION_COUNT;
}

const char *scale_direction_name(ScaleDirection direction) {
    return scale_direction_valid(direction) ? NAMES[direction] : NULL;
}

