#include "interpretation/chord.h"

#include <stdlib.h>
#include <string.h>

struct Chord {
    Interpretation *interpretation;
    char *label;
};

ChordResult chord_create(Interpretation *interpretation, const char *label, Chord **out) {
    if (interpretation == NULL) {
        return CHORD_ERR_NULL_INTERPRETATION;
    }
    if (interpretation_framework_reference(interpretation) == NULL) {
        return CHORD_ERR_NULL_FRAMEWORK_REFERENCE;
    }
    if (label == NULL || label[0] == '\0') {
        return CHORD_ERR_NULL_LABEL;
    }

    Chord *chord = (Chord *)malloc(sizeof(Chord));
    if (chord == NULL) {
        return CHORD_ERR_ALLOC;
    }

    char *owned_label = (char *)malloc(strlen(label) + 1);
    if (owned_label == NULL) {
        free(chord);
        return CHORD_ERR_ALLOC;
    }
    strcpy(owned_label, label);

    chord->interpretation = interpretation;
    chord->label = owned_label;

    *out = chord;
    return CHORD_OK;
}

void chord_destroy(Chord *chord) {
    if (chord == NULL) {
        return;
    }
    interpretation_destroy(chord->interpretation);
    free(chord->label);
    free(chord);
}

const Interpretation *chord_interpretation(const Chord *chord) {
    return chord->interpretation;
}

const char *chord_label(const Chord *chord) {
    return chord->label;
}