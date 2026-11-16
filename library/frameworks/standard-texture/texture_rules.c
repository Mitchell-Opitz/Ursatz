#include "texture_rules.h"

#include <stddef.h>
#include <string.h>

static bool valid_texture(StandardTexture texture) {
    return texture >= 0 && texture < STANDARD_TEXTURE_COUNT;
}

static const char *const TEXTURE_NAMES[STANDARD_TEXTURE_COUNT] = {
    "monophony", "heterophony", "homophony", "homorhythm", "melody and accompaniment", "polyphony"};

const char *standard_texture_name(StandardTexture texture) {
    return valid_texture(texture) ? TEXTURE_NAMES[texture] : NULL;
}

bool standard_texture_from_name(const char *name, StandardTexture *out) {
    if (name == NULL || out == NULL) {
        return false;
    }
    for (int t = 0; t < STANDARD_TEXTURE_COUNT; t++) {
        if (strcmp(name, TEXTURE_NAMES[t]) == 0) {
            *out = (StandardTexture)t;
            return true;
        }
    }
    return false;
}

bool standard_texture_is_homophonic(StandardTexture texture) {
    return texture == STANDARD_TEXTURE_HOMOPHONY || texture == STANDARD_TEXTURE_HOMORHYTHM ||
           texture == STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT;
}

bool standard_texture_category(StandardTexture texture, StandardTexture *category) {
    if (!valid_texture(texture) || category == NULL) {
        return false;
    }
    *category = standard_texture_is_homophonic(texture) ? STANDARD_TEXTURE_HOMOPHONY : texture;
    return true;
}

