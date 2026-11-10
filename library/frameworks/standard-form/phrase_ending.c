#include "phrase_ending.h"

#include <stddef.h>
#include <string.h>

static const char *const NAMES[PHRASE_ENDING_COUNT] = {"consequent", "continuation", "cadential"};

bool phrase_ending_valid(PhraseEnding value) {
    return value >= 0 && value < PHRASE_ENDING_COUNT;
}

const char *phrase_ending_name(PhraseEnding value) {
    return phrase_ending_valid(value) ? NAMES[value] : NULL;
}

bool phrase_ending_from_name(const char *name, PhraseEnding *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (int i = 0; i < PHRASE_ENDING_COUNT; i++) {
        if (strcmp(NAMES[i], name) == 0) {
            *out = (PhraseEnding)i;
            return true;
        }
    }
    return false;
}
