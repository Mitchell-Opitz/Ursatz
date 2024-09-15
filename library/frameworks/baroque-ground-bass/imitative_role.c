#include "imitative_role.h"

#include <stddef.h>

static const char *const NAMES[IMITATIVE_ROLE_COUNT] = {"dux", "comes"};

bool imitative_role_valid(ImitativeRole role) {
    return role >= 0 && role < IMITATIVE_ROLE_COUNT;
}

const char *imitative_role_name(ImitativeRole role) {
    return imitative_role_valid(role) ? NAMES[role] : NULL;
}
