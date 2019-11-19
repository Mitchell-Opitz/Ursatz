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

static PitchIdentity *clone_pitch_identity(const PitchIdentity *source) {
    size_t size = 0;
    const void *token = pitch_identity_token(source, &size);
    PitchIdentity *clone = NULL;
    if (pitch_identity_create(token, size, &clone) != PITCH_IDENTITY_OK) {
        return NULL;
    }
    return clone;
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

/* Builds a new NoteEvent identical to source except for temporal_anchor
   (a fresh MusicalTime at new_onset_numerator/new_onset_denominator).
   Returns NULL on any allocation failure. */
static NoteEvent *clone_note_event_with_new_onset(const NoteEvent *source, long new_onset_numerator,
                                                   unsigned long new_onset_denominator, const char *id_suffix) {
    MusicalTime *position = NULL;
    if (musical_time_create(new_onset_numerator, new_onset_denominator, &position) != MUSICAL_TIME_OK) {
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        return NULL;
    }

    const Duration *source_duration = note_event_duration(source);
    Duration *duration = NULL;
    if (duration_create(duration_numerator(source_duration), duration_denominator(source_duration), &duration) !=
        DURATION_OK) {
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchIdentity *pitch = clone_pitch_identity(note_event_pitch(source));
    if (pitch == NULL) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    EntityID *id = make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    ProvenanceReference *provenance = make_provenance_from(note_event_id(source));
    if (provenance == NULL) {
        entity_id_destroy(id);
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    NoteEvent *derived = NULL;
    if (note_event_create(id, anchor, duration, pitch, spelling, voice, provenance, &derived) != NOTE_EVENT_OK) {
        return NULL;
    }
    return derived;
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

    long new_onset_n;
    unsigned long new_onset_d;
    if (displacement->direction == DISPLACEMENT_DIRECTION_FORWARD) {
        new_onset_n = onset_n * (long)magnitude_d + magnitude_n * (long)onset_d;
    } else {
        new_onset_n = onset_n * (long)magnitude_d - magnitude_n * (long)onset_d;
    }
    new_onset_d = onset_d * magnitude_d;
    rational_reduce(&new_onset_n, &new_onset_d);

    NoteEvent *derived = clone_note_event_with_new_onset(event, new_onset_n, new_onset_d, "-displaced");
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