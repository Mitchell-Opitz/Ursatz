#include "motive_kind.h"

#include <stddef.h>

static const char *const NAMES[MOTIVE_KIND_COUNT] = {"pitch", "rhythmic", "contour", "timbre"};

bool motive_kind_valid(MotiveKind kind) {
    return kind >= 0 && kind < MOTIVE_KIND_COUNT;
}

const char *motive_kind_name(MotiveKind kind) {
    return motive_kind_valid(kind) ? NAMES[kind] : NULL;
}
