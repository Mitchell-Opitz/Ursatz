#include "semantics/scale.h"

#include <stdlib.h>

struct Scale {
    EntityID *tuning_system_id;
    PitchClass **pitch_classes;
    size_t pitch_class_count;
};

ScaleResult scale_create(EntityID *tuning_system_id, PitchClass **pitch_classes, size_t pitch_class_count,
                          Scale **out) {
    if (tuning_system_id == NULL) {
        return SCALE_ERR_NULL_TUNING_SYSTEM_ID;
    }
    if (pitch_classes == NULL || pitch_class_count == 0) {
        return SCALE_ERR_NULL_PITCH_CLASSES;
    }
    for (size_t i = 0; i < pitch_class_count; i++) {
        if (pitch_classes[i] == NULL) {
            return SCALE_ERR_NULL_PITCH_CLASS;
        }
    }

    Scale *scale = (Scale *)malloc(sizeof(Scale));
    if (scale == NULL) {
        return SCALE_ERR_ALLOC;
    }

    PitchClass **owned_pitch_classes = (PitchClass **)malloc(sizeof(PitchClass *) * pitch_class_count);
    if (owned_pitch_classes == NULL) {
        free(scale);
        return SCALE_ERR_ALLOC;
    }
    for (size_t i = 0; i < pitch_class_count; i++) {
        owned_pitch_classes[i] = pitch_classes[i];
    }

    scale->tuning_system_id = tuning_system_id;
    scale->pitch_classes = owned_pitch_classes;
    scale->pitch_class_count = pitch_class_count;

    *out = scale;
    return SCALE_OK;
}

void scale_destroy(Scale *scale) {
    if (scale == NULL) {
        return;
    }
    entity_id_destroy(scale->tuning_system_id);
    for (size_t i = 0; i < scale->pitch_class_count; i++) {
        pitch_class_destroy(scale->pitch_classes[i]);
    }
    free(scale->pitch_classes);
    free(scale);
}

const EntityID *scale_tuning_system_id(const Scale *scale) {
    return scale->tuning_system_id;
}

size_t scale_pitch_class_count(const Scale *scale) {
    return scale->pitch_class_count;
}

const PitchClass *scale_pitch_class_at(const Scale *scale, size_t index) {
    return scale->pitch_classes[index];
}