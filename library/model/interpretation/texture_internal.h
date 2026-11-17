#ifndef MUSIC_INTERPRETATION_TEXTURE_INTERNAL_H
#define MUSIC_INTERPRETATION_TEXTURE_INTERNAL_H

#include "model/interpretation/texture.h"

/* The fields of a Texture: texture.c writes them; tests read them through tests/support/interpretation_test_access.h. */
struct Texture {
    Interpretation *interpretation;
    char *label;
};

#endif
