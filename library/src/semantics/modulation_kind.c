#include "semantics/modulation_kind.h"

#include <stddef.h>

static const char *const NAMES[MODULATION_KIND_COUNT] = {"direct", "pivot"};

bool modulation_kind_valid(ModulationKind kind) {
    return kind >= 0 && kind < MODULATION_KIND_COUNT;
}

const char *modulation_kind_name(ModulationKind kind) {
    return modulation_kind_valid(kind) ? NAMES[kind] : NULL;
}
