#include "internal_expansion_technique.h"

#include <stddef.h>

static const char *const NAMES[INTERNAL_EXPANSION_TECHNIQUE_COUNT] = {"repetition", "stretching", "one-more-time", "alternative-path"};

bool internal_expansion_technique_valid(InternalExpansionTechnique value) {
    return value >= 0 && value < INTERNAL_EXPANSION_TECHNIQUE_COUNT;
}

const char *internal_expansion_technique_name(InternalExpansionTechnique value) {
    return internal_expansion_technique_valid(value) ? NAMES[value] : NULL;
}
