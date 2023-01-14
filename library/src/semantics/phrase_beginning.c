#include "semantics/phrase_beginning.h"

#include <stddef.h>

static const char *const NAMES[PHRASE_BEGINNING_COUNT] = {"antecedent", "compound-basic-idea", "presentation"};

bool phrase_beginning_valid(PhraseBeginning value) {
    return value >= 0 && value < PHRASE_BEGINNING_COUNT;
}

const char *phrase_beginning_name(PhraseBeginning value) {
    return phrase_beginning_valid(value) ? NAMES[value] : NULL;
}
