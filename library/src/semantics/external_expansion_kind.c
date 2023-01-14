#include "semantics/external_expansion_kind.h"

#include <stddef.h>

static const char *const NAMES[EXTERNAL_EXPANSION_KIND_COUNT] = {"prefix", "post-cadential-extension", "codetta", "coda"};

bool external_expansion_kind_valid(ExternalExpansionKind value) {
    return value >= 0 && value < EXTERNAL_EXPANSION_KIND_COUNT;
}

const char *external_expansion_kind_name(ExternalExpansionKind value) {
    return external_expansion_kind_valid(value) ? NAMES[value] : NULL;
}
