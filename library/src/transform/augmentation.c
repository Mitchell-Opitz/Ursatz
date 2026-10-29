#include "transform/augmentation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/transform_support.h"

struct Augmentation {
    Transformation *transformation;
    AugmentationDimension dimension;
    unsigned long scale_numerator;
    unsigned long scale_denominator;
};

AugmentationResult augmentation_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                        unsigned long scale_denominator, ProvenanceReference *provenance,
                                        Augmentation **out) {
    if (id == NULL) {
        return AUGMENTATION_ERR_NULL_ID;
    }
    const char *dimension_label = transform_support_dimension_name(dimension);
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

AugmentationApplyResult augmentation_apply(const Augmentation *augmentation, const NoteEvent *event,
                                            TransformResult **out) {
    if (augmentation == NULL) {
        return AUGMENTATION_APPLY_ERR_NULL_AUGMENTATION;
    }
    if (event == NULL) {
        return AUGMENTATION_APPLY_ERR_NULL_EVENT;
    }
    if (augmentation->dimension != AUGMENTATION_DIMENSION_DURATIONS) {
        return AUGMENTATION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED;
    }

    const Duration *source_duration = note_event_duration(event);
    long new_numerator = 0;
    unsigned long new_denominator = 1;
    if (!transform_support_rational_scale(duration_numerator(source_duration), duration_denominator(source_duration),
                                          augmentation->scale_numerator, augmentation->scale_denominator, &new_numerator,
                                          &new_denominator)) {
        return AUGMENTATION_APPLY_ERR_ALLOC;
    }

    NoteEvent *derived = transform_support_clone_note_event_with_new_duration(event, new_numerator, new_denominator, "-augmented");
    if (derived == NULL) {
        return AUGMENTATION_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return AUGMENTATION_APPLY_ERR_ALLOC;
    }
    return AUGMENTATION_APPLY_OK;
}