#include "semantics/subphrase_role.h"

#include <stddef.h>

static const char *const NAMES[SUBPHRASE_ROLE_COUNT] = {"presentation", "continuation"};

bool subphrase_role_valid(SubphraseRole value) {
    return value >= 0 && value < SUBPHRASE_ROLE_COUNT;
}

const char *subphrase_role_name(SubphraseRole value) {
    return subphrase_role_valid(value) ? NAMES[value] : NULL;
}
