#include "semantics/fugue_role.h"

#include <stddef.h>

static const char *const NAMES[FUGUE_ROLE_COUNT] = {"subject", "answer", "countersubject", "free-counterpoint",
                                                    "link", "episode"};

bool fugue_role_valid(FugueRole role) {
    return role >= 0 && role < FUGUE_ROLE_COUNT;
}

const char *fugue_role_name(FugueRole role) {
    return fugue_role_valid(role) ? NAMES[role] : NULL;
}

bool fugue_role_is_entry(FugueRole role) {
    return role == FUGUE_ROLE_SUBJECT || role == FUGUE_ROLE_ANSWER;
}
