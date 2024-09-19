#include "alternative_path_kind.h"

#include <stddef.h>

static const char *const NAMES[ALTERNATIVE_PATH_KIND_COUNT] = {"detour", "reroute"};

bool alternative_path_kind_valid(AlternativePathKind value) {
    return value >= 0 && value < ALTERNATIVE_PATH_KIND_COUNT;
}

const char *alternative_path_kind_name(AlternativePathKind value) {
    return alternative_path_kind_valid(value) ? NAMES[value] : NULL;
}
