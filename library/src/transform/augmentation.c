#include "transform/augmentation.h"

#include <stdlib.h>

struct Augmentation {
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

AugmentationResult augmentation_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                        unsigned long scale_denominator, ProvenanceReference *provenance,
                                        Augmentation **out) {
    if (id == NULL) {
        return AUGMENTATION_ERR_NULL_ID;
    }
    const char *dimension_label = dimension_name(dimension);
    if (dimension_label == NULL) {
        return AUGMENTATION_ERR_UNKNOWN_DIMENSION;
    }
    if (scale_denominator == 0) {
        return AUGMENTATION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (scale_numerator <= scale_denominator) {
        return AUGMENTATION_ERR_FACTOR_NOT_GREATER_THAN_ONE;
    }
    if (provenance == NULL) {
        return AUGMENTATION_ERR_NULL_PROVENANCE;
    }

    Augmentation *augmentation = (Augmentation *)malloc(sizeof(Augmentation));
    if (augmentation == NULL) {
        return AUGMENTATION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "pitch, event order, articulation", dimension_label,
        "one-to-one, scales the selected timing dimension by the given rational factor",
        "scale factor requires numerator > denominator (net lengthening); dimension explicitly declared, never "
        "defaulted to performance timing",
        true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(augmentation);
        return AUGMENTATION_ERR_ALLOC;
    }

    augmentation->transformation = transformation;
    augmentation->dimension = dimension;
    augmentation->scale_numerator = scale_numerator;
    augmentation->scale_denominator = scale_denominator;

    *out = augmentation;
    return AUGMENTATION_OK;
}

void augmentation_destroy(Augmentation *augmentation) {
    if (augmentation == NULL) {
        return;
    }
    transformation_destroy(augmentation->transformation);
    free(augmentation);
}

const EntityID *augmentation_id(const Augmentation *augmentation) {
    return transformation_id(augmentation->transformation);
}

const Transformation *augmentation_transformation(const Augmentation *augmentation) {
    return augmentation->transformation;
}

AugmentationDimension augmentation_dimension(const Augmentation *augmentation) {
    return augmentation->dimension;
}

unsigned long augmentation_scale_numerator(const Augmentation *augmentation) {
    return augmentation->scale_numerator;
}

unsigned long augmentation_scale_denominator(const Augmentation *augmentation) {
    return augmentation->scale_denominator;
}

bool augmentation_identity_equals(const Augmentation *a, const Augmentation *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}