#include "phrase_length_change.h"

#include <stddef.h>

static const char *const NAMES[PHRASE_LENGTH_CHANGE_COUNT] = {"expansion", "contraction"};

bool phrase_length_change_valid(PhraseLengthChange value) {
    return value >= 0 && value < PHRASE_LENGTH_CHANGE_COUNT;
}

const char *phrase_length_change_name(PhraseLengthChange value) {
    return phrase_length_change_valid(value) ? NAMES[value] : NULL;
}
