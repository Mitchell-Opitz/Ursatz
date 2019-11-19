#include "pitch/pitch_class.h"

#include <stdlib.h>

struct PitchClass {
    PitchIdentity *representative;
    PitchClassEquivalenceFn equivalence;
};

PitchClassResult pitch_class_create(PitchIdentity *representative, PitchClassEquivalenceFn equivalence,
                                     PitchClass **out) {
    if (representative == NULL) {
        return PITCH_CLASS_ERR_NULL_REPRESENTATIVE;
    }
    if (equivalence == NULL) {
        return PITCH_CLASS_ERR_NULL_EQUIVALENCE;
    }

    PitchClass *pitch_class = (PitchClass *)malloc(sizeof(PitchClass));
    if (pitch_class == NULL) {
        return PITCH_CLASS_ERR_ALLOC;
    }

    pitch_class->representative = representative;
    pitch_class->equivalence = equivalence;

    *out = pitch_class;
    return PITCH_CLASS_OK;
}

void pitch_class_destroy(PitchClass *pitch_class) {
    if (pitch_class == NULL) {
        return;
    }
    pitch_identity_destroy(pitch_class->representative);
    free(pitch_class);
}

const PitchIdentity *pitch_class_representative(const PitchClass *pitch_class) {
    return pitch_class->representative;
}

PitchClassEquivalenceFn pitch_class_equivalence(const PitchClass *pitch_class) {
    return pitch_class->equivalence;
}

bool pitch_class_equals(const PitchClass *a, const PitchClass *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    if (a->equivalence != b->equivalence) {
        return false;
    }
    return a->equivalence(a->representative, b->representative);
}