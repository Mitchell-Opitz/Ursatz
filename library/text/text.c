#include "text/text.h"

#include <string.h>

static const char *const kKindNames[] = {"json", "plan", "reading", "realization", "knowledge", "knowledge-entry", "commands", "view", "blueprint"};

const char *text_kind_name(TextKind kind) {
    if ((int)kind < 0 || (size_t)kind >= sizeof kKindNames / sizeof kKindNames[0]) {
        return "json";
    }
    return kKindNames[kind];
}

bool text_kind_find(const char *name, TextKind *kind) {
    if (!name) {
        return false;
    }
    for (size_t i = 0; i < sizeof kKindNames / sizeof kKindNames[0]; i++) {
        if (strcmp(name, kKindNames[i]) == 0) {
            if (kind) {
                *kind = (TextKind)i;
            }
            return true;
        }
    }
    return false;
}
