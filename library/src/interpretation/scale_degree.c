#include "interpretation/scale_degree.h"

#include <stdlib.h>
#include <string.h>

struct ScaleDegree {
    Interpretation *interpretation;
    PitchIdentity *pitch;
    char *label;
};

ScaleDegreeResult scale_degree_create(Interpretation *interpretation, PitchIdentity *pitch, const char *label,
                                       ScaleDegree **out) {
    if (interpretation == NULL) {
        return SCALE_DEGREE_ERR_NULL_INTERPRETATION;
    }
    if (interpretation_framework_reference(interpretation) == NULL) {
        return SCALE_DEGREE_ERR_NULL_FRAMEWORK_REFERENCE;
    }
    if (pitch == NULL) {
        return SCALE_DEGREE_ERR_NULL_PITCH;
    }
    if (label == NULL || label[0] == '\0') {
        return SCALE_DEGREE_ERR_NULL_LABEL;
    }

    ScaleDegree *scale_degree = (ScaleDegree *)malloc(sizeof(ScaleDegree));
    if (scale_degree == NULL) {
        return SCALE_DEGREE_ERR_ALLOC;
    }

    char *owned_label = (char *)malloc(strlen(label) + 1);
    if (owned_label == NULL) {
        free(scale_degree);
        return SCALE_DEGREE_ERR_ALLOC;
    }
    strcpy(owned_label, label);

    scale_degree->interpretation = interpretation;
    scale_degree->pitch = pitch;
    scale_degree->label = owned_label;

    *out = scale_degree;
    return SCALE_DEGREE_OK;
}

void scale_degree_destroy(ScaleDegree *scale_degree) {
    if (scale_degree == NULL) {
        return;
    }
    interpretation_destroy(scale_degree->interpretation);
    pitch_identity_destroy(scale_degree->pitch);
    free(scale_degree->label);
    free(scale_degree);
}

const Interpretation *scale_degree_interpretation(const ScaleDegree *scale_degree) {
    return scale_degree->interpretation;
}

const PitchIdentity *scale_degree_pitch(const ScaleDegree *scale_degree) {
    return scale_degree->pitch;
}

const char *scale_degree_label(const ScaleDegree *scale_degree) {
    return scale_degree->label;
}