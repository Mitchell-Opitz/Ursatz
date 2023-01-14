#include "semantics/phrase_role.h"

#include <stddef.h>

static const char *const NAMES[PHRASE_ROLE_COUNT] = {"antecedent", "consequent"};

bool phrase_role_valid(PhraseRole value) {
    return value >= 0 && value < PHRASE_ROLE_COUNT;
}

const char *phrase_role_name(PhraseRole value) {
    return phrase_role_valid(value) ? NAMES[value] : NULL;
}
