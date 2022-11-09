#include "semantics/applied_chord_kind.h"

#include <stddef.h>

static const char *const NAMES[APPLIED_CHORD_KIND_COUNT] = {"secondary-dominant", "secondary-leading-tone"};

bool applied_chord_kind_valid(AppliedChordKind kind) {
    return kind >= 0 && kind < APPLIED_CHORD_KIND_COUNT;
}

const char *applied_chord_kind_name(AppliedChordKind kind) {
    return applied_chord_kind_valid(kind) ? NAMES[kind] : NULL;
}
