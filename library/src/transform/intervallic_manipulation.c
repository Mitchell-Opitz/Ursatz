#include "transform/intervallic_manipulation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/transform_support.h"

struct IntervallicManipulation {
    Transformation *transformation;
    long *adjustments;
    size_t adjustment_count;
};

IntervallicManipulationResult intervallic_manipulation_create(EntityID *id, const long *adjustments,
                                                                size_t adjustment_count,
                                                                ProvenanceReference *provenance,
                                                                IntervallicManipulation **out) {
    if (id == NULL) {
        return INTERVALLIC_MANIPULATION_ERR_NULL_ID;
    }
    if (adjustments == NULL || adjustment_count == 0) {
        return INTERVALLIC_MANIPULATION_ERR_NULL_ADJUSTMENTS;
    }
    if (provenance == NULL) {
        return INTERVALLIC_MANIPULATION_ERR_NULL_PROVENANCE;
    }

    IntervallicManipulation *manipulation = (IntervallicManipulation *)malloc(sizeof(IntervallicManipulation));
    if (manipulation == NULL) {
        return INTERVALLIC_MANIPULATION_ERR_ALLOC;
    }
    manipulation->adjustments = (long *)malloc(sizeof(long) * adjustment_count);
    if (manipulation->adjustments == NULL) {
        free(manipulation);
        return INTERVALLIC_MANIPULATION_ERR_ALLOC;
    }
    memcpy(manipulation->adjustments, adjustments, sizeof(long) * adjustment_count);

    Value *parameters = NULL;
    if (value_create("interval-adjustments", manipulation->adjustments, sizeof(long) * adjustment_count,
                     &parameters) != VALUE_OK) {
        free(manipulation->adjustments);
        free(manipulation);
        return INTERVALLIC_MANIPULATION_ERR_ALLOC;
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", parameters, "NoteEvent", "rhythm, event order, articulation, contour",
        "pitch (interval sizes)",
        "sequence-level, one output event per input event; each interval is widened or narrowed in its own "
        "direction and the change carries on to the notes after it",
        "adjustment count must equal the event count minus one", true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        value_destroy(parameters);
        free(manipulation->adjustments);
        free(manipulation);
        return INTERVALLIC_MANIPULATION_ERR_ALLOC;
    }

    manipulation->transformation = transformation;
    manipulation->adjustment_count = adjustment_count;

    *out = manipulation;
    return INTERVALLIC_MANIPULATION_OK;
}

void intervallic_manipulation_destroy(IntervallicManipulation *manipulation) {
    if (manipulation == NULL) {
        return;
    }
    transformation_destroy(manipulation->transformation);
    free(manipulation->adjustments);
    free(manipulation);
}

const EntityID *intervallic_manipulation_id(const IntervallicManipulation *manipulation) {
    return transformation_id(manipulation->transformation);
}

const Transformation *intervallic_manipulation_transformation(const IntervallicManipulation *manipulation) {
    return manipulation->transformation;
}

size_t intervallic_manipulation_adjustment_count(const IntervallicManipulation *manipulation) {
    return manipulation->adjustment_count;
}

long intervallic_manipulation_adjustment_at(const IntervallicManipulation *manipulation, size_t index) {
    return manipulation->adjustments[index];
}

bool intervallic_manipulation_identity_equals(const IntervallicManipulation *a, const IntervallicManipulation *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

static void destroy_events(NoteEvent **events, size_t count) {
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);
}

IntervallicManipulationApplyResult intervallic_manipulation_apply(const IntervallicManipulation *manipulation,
                                                                    NoteEvent **events,
                                                                    const PitchNumericValue **pitch_values,
                                                                    size_t event_count, TransformResult **out) {
    if (manipulation == NULL) {
        return INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_MANIPULATION;
    }
    if (events == NULL) {
        return INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_EVENTS;
    }
    if (pitch_values == NULL) {
        return INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_PITCH_VALUES;
    }
    if (event_count == 0) {
        return INTERVALLIC_MANIPULATION_APPLY_ERR_EMPTY;
    }
    if (manipulation->adjustment_count != event_count - 1) {
        return INTERVALLIC_MANIPULATION_APPLY_ERR_ADJUSTMENT_COUNT_MISMATCH;
    }

    NoteEvent **derived = (NoteEvent **)calloc(event_count, sizeof(NoteEvent *));
    if (derived == NULL) {
        return INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC;
    }

    long offset = 0;
    for (size_t i = 0; i < event_count; i++) {
        if (i > 0) {
            long step_n = 0;
            unsigned long step_d = 1;
            if (pitch_numeric_value_distance(pitch_values[i], pitch_values[i - 1], &step_n, &step_d) !=
                PITCH_NUMERIC_VALUE_OP_OK) {
                destroy_events(derived, i);
                return INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC;
            }
            long adjustment = manipulation->adjustments[i - 1];
            if (step_n > 0) {
                offset += adjustment;
            } else if (step_n < 0) {
                offset -= adjustment;
            }
        }

        PitchNumericValue *shifted = NULL;
        if (pitch_numeric_value_add_interval(pitch_values[i], offset, 1, &shifted) != PITCH_NUMERIC_VALUE_OP_OK) {
            destroy_events(derived, i);
            return INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC;
        }
        PitchIdentity *new_pitch = NULL;
        PitchNumericValueToIdentityResult identity_result = pitch_numeric_value_to_identity(shifted, &new_pitch);
        pitch_numeric_value_destroy(shifted);
        if (identity_result != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK) {
            destroy_events(derived, i);
            return INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC;
        }

        derived[i] = transform_support_clone_note_event_with_new_pitch(events[i], new_pitch, "-intervallic");
        if (derived[i] == NULL) {
            destroy_events(derived, i);
            return INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC;
        }
    }

    TransformResultResult result = transform_result_create(derived, event_count, out);
    if (result != TRANSFORM_RESULT_OK) {
        destroy_events(derived, event_count);
        return INTERVALLIC_MANIPULATION_APPLY_ERR_ALLOC;
    }
    free(derived);
    return INTERVALLIC_MANIPULATION_APPLY_OK;
}
