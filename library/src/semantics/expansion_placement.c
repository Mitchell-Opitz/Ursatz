#include "semantics/expansion_placement.h"

#include <stddef.h>

static const char *const NAMES[EXPANSION_PLACEMENT_COUNT] = {"internal", "external"};

bool expansion_placement_valid(ExpansionPlacement value) {
    return value >= 0 && value < EXPANSION_PLACEMENT_COUNT;
}

const char *expansion_placement_name(ExpansionPlacement value) {
    return expansion_placement_valid(value) ? NAMES[value] : NULL;
}
