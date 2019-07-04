#include "transform/diminution.h"

#include <stdlib.h>

struct Diminution {
    Transformation *transformation;
    AugmentationDimension dimension;
    unsigned long scale_numerator;
    unsigned long scale_denominator;
};

static const char *dimension_name(AugmentationDimension dimension) {
    switch (dimension) {
        case AUGMENTATION_DIMENSION_DURATIONS:
            return "durations";
        case AUGMENTATION_DIMENSION_INTER_ONSET_INTERVALS:
            return "inter-onset intervals";
        case AUGMENTATION_DIMENSION_STRUCTURAL_TIMING:
            return "structural timing";
        case AUGMENTATION_DIMENSION_NOTATION_VALUES:
            return "notation values";
        case AUGMENTATION_DIMENSION_PERFORMANCE_TIMING:
            return "performance timing";
        default:
            return NULL;
    }
}

DiminutionResult diminution_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                    unsigned long scale_denominator, ProvenanceReference *provenance,
                                    Diminution **out) {
    if (id == NULL) {
        return DIMINUTION_ERR_NULL_ID;
    }
    const char *dimension_label = dimension_name(dimension);
    if (dimension_label == NULL) {
        return DIMINUTION_ERR_UNKNOWN_DIMENSION;
    }
    if (scale_numerator == 0 || scale_denominator == 0) {
        return DIMINUTION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (scale_numerator >= scale_denominator) {
        return DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE;
    }
    if (provenance == NULL) {
        return DIMINUTION_ERR_NULL_PROVENANCE;
    }

    Diminution *diminution = (Diminution *)malloc(sizeof(Diminution));
    if (diminution == NULL) {
        return DIMINUTION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "pitch, event order, articulation", dimension_label,
        "one-to-one, scales the selected timing dimension by the given rational factor",
        "scale factor requires 0 < numerator < denominator (net shrinking); dimension explicitly declared, never "
        "defaulted to performance timing",
        true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(diminution);
        return DIMINUTION_ERR_ALLOC;
    }

    diminution->transformation = transformation;
    diminution->dimension = dimension;
    diminution->scale_numerator = scale_numerator;
    diminution->scale_denominator = scale_denominator;

    *out = diminution;
    return DIMINUTION_OK;
}

void diminution_destroy(Diminution *diminution) {
    if (diminution == NULL) {
        return;
    }
    transformation_destroy(diminution->transformation);
    free(diminution);
}

const EntityID *diminution_id(const Diminution *diminution) {
    return transformation_id(diminution->transformation);
}

const Transformation *diminution_transformation(const Diminution *diminution) {
    return diminution->transformation;
}

AugmentationDimension diminution_dimension(const Diminution *diminution) {
    return diminution->dimension;
}

unsigned long diminution_scale_numerator(const Diminution *diminution) {
    return diminution->scale_numerator;
}

unsigned long diminution_scale_denominator(const Diminution *diminution) {
    return diminution->scale_denominator;
}

bool diminution_identity_equals(const Diminution *a, const Diminution *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}