#ifndef MUSIC_INTERPRETATION_KEY_INTERNAL_H
#define MUSIC_INTERPRETATION_KEY_INTERNAL_H

#include "interpretation/key.h"

/* The fields of a Key: key.c writes them; tests read them through tests/support/interpretation_test_access.h. */
struct Key {
    Interpretation *interpretation;
    char *label;
    bool has_structure;
    KeyStructure structure;
};

#endif
