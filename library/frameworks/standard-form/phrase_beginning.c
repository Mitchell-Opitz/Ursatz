#include "phrase_beginning.h"

#include <stddef.h>
#include <string.h>

static const char *const NAMES[PHRASE_BEGINNING_COUNT] = {"antecedent", "compound-basic-idea", "presentation"};

bool phrase_beginning_valid(PhraseBeginning value) {
    return value >= 0 && value < PHRASE_BEGINNING_COUNT;
}

const char *phrase_beginning_name(PhraseBeginning value) {
    return phrase_beginning_valid(value) ? NAMES[value] : NULL;
}

bool phrase_beginning_from_name(const char *name, PhraseBeginning *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (int i = 0; i < PHRASE_BEGINNING_COUNT; i++) {
        if (strcmp(NAMES[i], name) == 0) {
            *out = (PhraseBeginning)i;
            return true;
        }
    }
    return false;
}
