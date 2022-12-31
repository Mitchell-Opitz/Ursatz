#include "semantics/phrase_ending.h"

#include <stddef.h>

static const char *const NAMES[PHRASE_ENDING_COUNT] = {"consequent", "continuation", "cadential"};

bool phrase_ending_valid(PhraseEnding value) {
    return value >= 0 && value < PHRASE_ENDING_COUNT;
}

const char *phrase_ending_name(PhraseEnding value) {
    return phrase_ending_valid(value) ? NAMES[value] : NULL;
}
