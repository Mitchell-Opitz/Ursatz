#include "transform/inversion.h"

#include <stdlib.h>
#include <string.h>

struct Inversion {
    Transformation *transformation;
    PitchIdentity *axis;
    char *pitch_space;
    char *spelling_policy;
    bool has_tuning_semantics;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

InversionResult inversion_create(EntityID *id, PitchIdentity *axis, const char *pitch_space,
                                  const char *spelling_policy, bool has_tuning_semantics,
                                  ProvenanceReference *provenance, Inversion **out) {
    if (id == NULL) {
        return INVERSION_ERR_NULL_ID;
    }
    if (axis == NULL) {
        return INVERSION_ERR_NULL_AXIS;
    }
    if (pitch_space == NULL || pitch_space[0] == '\0') {
        return INVERSION_ERR_NULL_PITCH_SPACE;
    }
    if (spelling_policy == NULL || spelling_policy[0] == '\0') {
        return INVERSION_ERR_NULL_SPELLING_POLICY;
    }
    if (has_tuning_semantics) {
        return INVERSION_ERR_TUNING_SEMANTICS_NOT_SUPPORTED;
    }
    if (provenance == NULL) {
        return INVERSION_ERR_NULL_PROVENANCE;
    }

    Inversion *inversion = (Inversion *)malloc(sizeof(Inversion));
    if (inversion == NULL) {
        return INVERSION_ERR_ALLOC;
    }

    inversion->pitch_space = copy_string(pitch_space);
    if (inversion->pitch_space == NULL) {
        free(inversion);
        return INVERSION_ERR_ALLOC;
    }

    inversion->spelling_policy = copy_string(spelling_policy);
    if (inversion->spelling_policy == NULL) {
        free(inversion->pitch_space);
        free(inversion);
        return INVERSION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result =
        transformation_create(id, "NoteEvent", NULL, "NoteEvent", "rhythm, event order, articulation", "pitch",
                               "one-to-one, per-event pitch reflection about axis",
                               "spelling policy applied per configured policy; tuning semantics not yet implemented "
                               "(Deliverable D)",
                               true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(inversion->spelling_policy);
        free(inversion->pitch_space);
        free(inversion);
        return INVERSION_ERR_ALLOC;
    }

    inversion->transformation = transformation;
    inversion->axis = axis;
    inversion->has_tuning_semantics = has_tuning_semantics;

    *out = inversion;
    return INVERSION_OK;
}

void inversion_destroy(Inversion *inversion) {
    if (inversion == NULL) {
        return;
    }
    transformation_destroy(inversion->transformation);
    pitch_identity_destroy(inversion->axis);
    free(inversion->pitch_space);
    free(inversion->spelling_policy);
    free(inversion);
}

const EntityID *inversion_id(const Inversion *inversion) {
    return transformation_id(inversion->transformation);
}

const Transformation *inversion_transformation(const Inversion *inversion) {
    return inversion->transformation;
}

const PitchIdentity *inversion_axis(const Inversion *inversion) {
    return inversion->axis;
}

const char *inversion_pitch_space(const Inversion *inversion) {
    return inversion->pitch_space;
}

const char *inversion_spelling_policy(const Inversion *inversion) {
    return inversion->spelling_policy;
}

bool inversion_has_tuning_semantics(const Inversion *inversion) {
    return inversion->has_tuning_semantics;
}

bool inversion_identity_equals(const Inversion *a, const Inversion *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}