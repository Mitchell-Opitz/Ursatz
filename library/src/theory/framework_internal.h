#ifndef MUSIC_THEORY_FRAMEWORK_INTERNAL_H
#define MUSIC_THEORY_FRAMEWORK_INTERNAL_H

#include "theory/framework.h"

/* The fields of a Framework: framework.c writes them; tests read them through tests/support/framework_test_access.h. */
struct Framework {
    EntityID *id;
    char *name;
};

#endif
