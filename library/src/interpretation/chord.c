#include "interpretation/chord.h"

#include <stdlib.h>
#include <string.h>

struct Chord {
    Interpretation *interpretation;
    char *label;
    bool has_structure;
    ChordStructure structure;
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
    chord->has_structure = false;

    *out = chord;
    return CHORD_OK;
}

ChordResult chord_create_with_structure(Interpretation *interpretation, const char *label,
                                         const ChordStructure *structure, Chord **out) {
    if (structure == NULL || structure->interval_count > CHORD_STRUCTURE_MAX_INTERVALS ||
        structure->root_pitch_class < -1 || structure->root_pitch_class > 11 || structure->bass_pitch_class < -1 ||
        structure->bass_pitch_class > 11) {
        return CHORD_ERR_BAD_STRUCTURE;
    }
    Chord *chord = NULL;
    ChordResult result = chord_create(interpretation, label, &chord);
    if (result != CHORD_OK) {
        return result;
    }
    chord->has_structure = true;
    chord->structure = *structure;
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

Interpretation *chord_take_interpretation(Chord *chord) {
    Interpretation *interpretation = chord->interpretation;
    free(chord->label);
    free(chord);
    return interpretation;
}

bool chord_structure(const Chord *chord, ChordStructure *out) {
    if (!chord->has_structure) {
        return false;
    }
    *out = chord->structure;
    return true;
}
