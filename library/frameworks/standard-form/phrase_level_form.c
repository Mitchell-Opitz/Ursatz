#include "phrase_level_form.h"

#include <stddef.h>
#include <string.h>

static const char *const NAMES[PHRASE_LEVEL_FORM_COUNT] = {"sentence", "period", "repeated-phrase", "compound-period", "hybrid", "unique"};

bool phrase_level_form_valid(PhraseLevelForm value) {
    return value >= 0 && value < PHRASE_LEVEL_FORM_COUNT;
}

const char *phrase_level_form_name(PhraseLevelForm value) {
    return phrase_level_form_valid(value) ? NAMES[value] : NULL;
}

bool phrase_level_form_from_name(const char *name, PhraseLevelForm *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (int i = 0; i < PHRASE_LEVEL_FORM_COUNT; i++) {
        if (strcmp(NAMES[i], name) == 0) {
            *out = (PhraseLevelForm)i;
            return true;
        }
    }
    return false;
}
