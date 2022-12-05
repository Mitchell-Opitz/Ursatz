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

static bool clone_pitch_spelling_or_null(const PitchSpelling *source, PitchSpelling **out) {
    if (source == NULL) {
        *out = NULL;
        return true;
    }
    return pitch_spelling_create(pitch_spelling_letter(source), pitch_spelling_accidental(source),
                                  pitch_spelling_octave(source), out) == PITCH_SPELLING_OK;
}

static bool clone_voice_reference_or_null(const VoiceReference *source, VoiceReference **out) {
    if (source == NULL) {
        *out = NULL;
        return true;
    }
    EntityID *target_copy = NULL;
    if (entity_id_create(entity_id_value(voice_reference_target_id(source)), &target_copy) != ENTITY_ID_OK) {
        return false;
    }
    if (voice_reference_create(target_copy, out) != VOICE_REFERENCE_OK) {
        entity_id_destroy(target_copy);
        return false;
    }
    return true;
}

static EntityID *make_derived_id(const EntityID *source_id, const char *suffix) {
    const char *source_value = entity_id_value(source_id);
    size_t len = strlen(source_value) + strlen(suffix) + 1;
    char *buffer = (char *)malloc(len);
    if (buffer == NULL) {
        return NULL;
    }
    snprintf(buffer, len, "%s%s", source_value, suffix);
    EntityID *derived = NULL;
    entity_id_create(buffer, &derived);
    free(buffer);
    return derived;
}

static ProvenanceReference *make_provenance_from(const EntityID *source_id) {
    EntityID *target_copy = NULL;
    if (entity_id_create(entity_id_value(source_id), &target_copy) != ENTITY_ID_OK) {
        return NULL;
    }
    ProvenanceReference *provenance = NULL;
    if (provenance_reference_create_present(target_copy, &provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(target_copy);
        return NULL;
    }
    return provenance;
}

/* Builds a new NoteEvent identical to source except for pitch (a newly
   constructed PitchIdentity, ownership transferred in). Returns NULL on
   any allocation failure -- new_pitch is destroyed on that path too. */
static NoteEvent *clone_note_event_with_new_pitch(const NoteEvent *source, PitchIdentity *new_pitch,
                                                   const char *id_suffix) {
    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(source));
    MusicalTime *position = NULL;
    if (musical_time_create(musical_time_numerator(source_position), musical_time_denominator(source_position),
                             &position) != MUSICAL_TIME_OK) {
        pitch_identity_destroy(new_pitch);
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    const Duration *source_duration = note_event_duration(source);
    Duration *duration = NULL;
    if (duration_create(duration_numerator(source_duration), duration_denominator(source_duration), &duration) !=
        DURATION_OK) {
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    EntityID *id = make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    ProvenanceReference *provenance = make_provenance_from(note_event_id(source));
    if (provenance == NULL) {
        entity_id_destroy(id);
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    NoteEvent *derived = NULL;
    if (note_event_create(id, anchor, duration, new_pitch, spelling, voice, provenance, &derived) != NOTE_EVENT_OK) {
        return NULL;
    }
    return derived;
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

        derived[i] = clone_note_event_with_new_pitch(events[i], new_pitch, "-intervallic");
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
