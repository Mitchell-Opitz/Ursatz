#include "transform/diminution.h"

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

struct Diminution {
    Transformation *transformation;
    AugmentationDimension dimension;
    unsigned long scale_numerator;
    unsigned long scale_denominator;
};

DiminutionResult diminution_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                    unsigned long scale_denominator, ProvenanceReference *provenance,
                                    Diminution **out) {
    if (id == NULL) {
        return DIMINUTION_ERR_NULL_ID;
    }
    const char *dimension_label = transform_support_dimension_name(dimension);
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

static unsigned long gcd_ul(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static void rational_reduce(long *numerator, unsigned long *denominator) {
    unsigned long abs_numerator = *numerator < 0 ? (unsigned long)(-*numerator) : (unsigned long)*numerator;
    unsigned long divisor = gcd_ul(abs_numerator, *denominator);
    if (divisor == 0) {
        divisor = 1;
    }
    *numerator = *numerator / (long)divisor;
    *denominator = *denominator / divisor;
}

DiminutionApplyResult diminution_apply(const Diminution *diminution, const NoteEvent *event, TransformResult **out) {
    if (diminution == NULL) {
        return DIMINUTION_APPLY_ERR_NULL_DIMINUTION;
    }
    if (event == NULL) {
        return DIMINUTION_APPLY_ERR_NULL_EVENT;
    }
    if (diminution->dimension != AUGMENTATION_DIMENSION_DURATIONS) {
        return DIMINUTION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED;
    }

    const Duration *source_duration = note_event_duration(event);
    long new_numerator = duration_numerator(source_duration) * (long)diminution->scale_numerator;
    unsigned long new_denominator = duration_denominator(source_duration) * diminution->scale_denominator;
    rational_reduce(&new_numerator, &new_denominator);

    NoteEvent *derived = transform_support_clone_note_event_with_new_duration(event, new_numerator, new_denominator, "-diminished");
    if (derived == NULL) {
        return DIMINUTION_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return DIMINUTION_APPLY_ERR_ALLOC;
    }
    return DIMINUTION_APPLY_OK;
}