#include "phrase_level_form.h"

#include <stddef.h>

static const char *const NAMES[PHRASE_LEVEL_FORM_COUNT] = {"sentence", "period", "repeated-phrase", "compound-period", "hybrid", "unique"};

bool phrase_level_form_valid(PhraseLevelForm value) {
    return value >= 0 && value < PHRASE_LEVEL_FORM_COUNT;
}

const char *phrase_level_form_name(PhraseLevelForm value) {
    return phrase_level_form_valid(value) ? NAMES[value] : NULL;
}
