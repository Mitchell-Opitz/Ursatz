#include "interpretation/key.h"

#include <stdlib.h>
#include <string.h>

struct Key {
    Interpretation *interpretation;
    char *label;
    bool has_structure;
    KeyStructure structure;
};

KeyResult key_create(Interpretation *interpretation, const char *label, Key **out) {
    if (interpretation == NULL) {
        return KEY_ERR_NULL_INTERPRETATION;
    }
    if (interpretation_framework_reference(interpretation) == NULL) {
        return KEY_ERR_NULL_FRAMEWORK_REFERENCE;
    }
    if (label == NULL || label[0] == '\0') {
        return KEY_ERR_NULL_LABEL;
    }

    Key *key = (Key *)malloc(sizeof(Key));
    if (key == NULL) {
        return KEY_ERR_ALLOC;
    }

    char *owned_label = (char *)malloc(strlen(label) + 1);
    if (owned_label == NULL) {
        free(key);
        return KEY_ERR_ALLOC;
    }
    strcpy(owned_label, label);

    key->interpretation = interpretation;
    key->label = owned_label;
    key->has_structure = false;

    *out = key;
    return KEY_OK;
}

KeyResult key_create_with_structure(Interpretation *interpretation, const char *label, const KeyStructure *structure,
                                     Key **out) {
    if (structure == NULL || structure->tonic_pitch_class < 0 || structure->tonic_pitch_class > 11 ||
        (structure->mode != KEY_STRUCTURE_MODE_MAJOR && structure->mode != KEY_STRUCTURE_MODE_MINOR)) {
        return KEY_ERR_BAD_STRUCTURE;
    }
    Key *key = NULL;
    KeyResult result = key_create(interpretation, label, &key);
    if (result != KEY_OK) {
        return result;
    }
    key->has_structure = true;
    key->structure = *structure;
    *out = key;
    return KEY_OK;
}

void key_destroy(Key *key) {
    if (key == NULL) {
        return;
    }
    interpretation_destroy(key->interpretation);
    free(key->label);
    free(key);
}

const Interpretation *key_interpretation(const Key *key) {
    return key->interpretation;
}

const char *key_label(const Key *key) {
    return key->label;
}

Interpretation *key_take_interpretation(Key *key) {
    Interpretation *interpretation = key->interpretation;
    free(key->label);
    free(key);
    return interpretation;
}

bool key_structure(const Key *key, KeyStructure *out) {
    if (!key->has_structure) {
        return false;
    }
    *out = key->structure;
    return true;
}
