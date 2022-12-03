#include "semantics/form_level.h"

#include <stddef.h>

static const char *const NAMES[FORM_LEVEL_COUNT] = {"piece",  "movement",  "section", "theme",
                                                    "phrase", "subphrase", "idea",    "motive"};

bool form_level_valid(FormLevel level) {
    return level >= 0 && level < FORM_LEVEL_COUNT;
}

const char *form_level_name(FormLevel level) {
    return form_level_valid(level) ? NAMES[level] : NULL;
}
