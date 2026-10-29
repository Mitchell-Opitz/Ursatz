#include "transform/displacement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/transform_support.h"

struct Displacement {
    Transformation *transformation;
    DisplacementDirection direction;
    Duration *magnitude;
};

static const char *direction_label(DisplacementDirection direction) {
    switch (direction) {
        case DISPLACEMENT_DIRECTION_FORWARD:
            return "forward";
        case DISPLACEMENT_DIRECTION_BACKWARD:
            return "backward";
        default:
            return NULL;
    }
}

DisplacementResult displacement_create(EntityID *id, DisplacementDirection direction, Duration *magnitude,
                                        ProvenanceReference *provenance, Displacement **out) {
    if (id == NULL) {
        return DISPLACEMENT_ERR_NULL_ID;
    }
    if (direction_label(direction) == NULL) {
        return DISPLACEMENT_ERR_UNKNOWN_DIRECTION;
    }
    if (magnitude == NULL) {
        return DISPLACEMENT_ERR_NULL_MAGNITUDE;
    }
    if (provenance == NULL) {
        return DISPLACEMENT_ERR_NULL_PROVENANCE;
    }

    Displacement *displacement = (Displacement *)malloc(sizeof(Displacement));
    if (displacement == NULL) {
        return DISPLACEMENT_ERR_ALLOC;
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "pitch, duration, articulation", "temporal position (TemporalAnchor)",
        "one-to-one, shifts only the event's TemporalAnchor by the configured direction and magnitude; every "
        "other field unchanged",
        "magnitude is non-negative (enforced by Duration itself); direction explicitly declared", true, provenance,
        &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(displacement);
        return DISPLACEMENT_ERR_ALLOC;
    }

    displacement->transformation = transformation;
    displacement->direction = direction;
    displacement->magnitude = magnitude;

    *out = displacement;
    return DISPLACEMENT_OK;
}

void displacement_destroy(Displacement *displacement) {
    if (displacement == NULL) {
        return;
    }
    transformation_destroy(displacement->transformation);
    duration_destroy(displacement->magnitude);
    free(displacement);
}

const EntityID *displacement_id(const Displacement *displacement) {
    return transformation_id(displacement->transformation);
}

const Transformation *displacement_transformation(const Displacement *displacement) {
    return displacement->transformation;
}

DisplacementDirection displacement_direction(const Displacement *displacement) {
    return displacement->direction;
}

const Duration *displacement_magnitude(const Displacement *displacement) {
    return displacement->magnitude;
}

bool displacement_identity_equals(const Displacement *a, const Displacement *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

DisplacementApplyResult displacement_apply(const Displacement *displacement, const NoteEvent *event,
                                            TransformResult **out) {
    if (displacement == NULL) {
        return DISPLACEMENT_APPLY_ERR_NULL_DISPLACEMENT;
    }
    if (event == NULL) {
        return DISPLACEMENT_APPLY_ERR_NULL_EVENT;
    }

    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(event));
    long onset_n = musical_time_numerator(source_position);
    unsigned long onset_d = musical_time_denominator(source_position);
    long magnitude_n = duration_numerator(displacement->magnitude);
    unsigned long magnitude_d = duration_denominator(displacement->magnitude);

    long new_onset_n = 0;
    unsigned long new_onset_d = 1;
    bool computed = displacement->direction == DISPLACEMENT_DIRECTION_FORWARD
                        ? transform_support_rational_add(onset_n, onset_d, magnitude_n, magnitude_d, &new_onset_n,
                                                         &new_onset_d)
                        : transform_support_rational_subtract(onset_n, onset_d, magnitude_n, magnitude_d,
                                                              &new_onset_n, &new_onset_d);
    if (!computed) {
        return DISPLACEMENT_APPLY_ERR_ALLOC;
    }

    NoteEvent *derived = transform_support_clone_note_event_with_new_onset(event, new_onset_n, new_onset_d, "-displaced");
    if (derived == NULL) {
        return DISPLACEMENT_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return DISPLACEMENT_APPLY_ERR_ALLOC;
    }
    return DISPLACEMENT_APPLY_OK;
}