#include "interpretation/texture.h"

#include <stdlib.h>
#include <string.h>

struct Texture {
    Interpretation *interpretation;
    char *label;
};

TextureResult texture_create(Interpretation *interpretation, const char *label, Texture **out) {
    if (interpretation == NULL) {
        return TEXTURE_ERR_NULL_INTERPRETATION;
    }
    if (interpretation_framework_reference(interpretation) == NULL) {
        return TEXTURE_ERR_NULL_FRAMEWORK_REFERENCE;
    }
    if (label == NULL || label[0] == '\0') {
        return TEXTURE_ERR_NULL_LABEL;
    }

    Texture *texture = (Texture *)malloc(sizeof(Texture));
    if (texture == NULL) {
        return TEXTURE_ERR_ALLOC;
    }

    char *owned_label = (char *)malloc(strlen(label) + 1);
    if (owned_label == NULL) {
        free(texture);
        return TEXTURE_ERR_ALLOC;
    }
    strcpy(owned_label, label);

    texture->interpretation = interpretation;
    texture->label = owned_label;

    *out = texture;
    return TEXTURE_OK;
}

void texture_destroy(Texture *texture) {
    if (texture == NULL) {
        return;
    }
    interpretation_destroy(texture->interpretation);
    free(texture->label);
    free(texture);
}

const Interpretation *texture_interpretation(const Texture *texture) {
    return texture->interpretation;
}

const char *texture_label(const Texture *texture) {
    return texture->label;
}

Interpretation *texture_take_interpretation(Texture *texture) {
    Interpretation *interpretation = texture->interpretation;
    free(texture->label);
    free(texture);
    return interpretation;
}