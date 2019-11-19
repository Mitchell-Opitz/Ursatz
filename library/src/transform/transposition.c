#include "transform/transposition.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

struct Transposition {
    Transformation *transformation;
    long semitones;
    char *spelling_behavior;
    bool has_tuning_behavior;
    bool has_range_policy;
    bool has_instrument_context;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

TranspositionResult transposition_create(EntityID *id, long semitones, const char *spelling_behavior,
                                          bool has_tuning_behavior, bool has_range_policy,
                                          bool has_instrument_context, ProvenanceReference *provenance,
                                          Transposition **out) {
    if (id == NULL) {
        return TRANSPOSITION_ERR_NULL_ID;
    }
    if (spelling_behavior == NULL || spelling_behavior[0] == '\0') {
        return TRANSPOSITION_ERR_NULL_SPELLING_BEHAVIOR;
    }
    if (has_tuning_behavior) {
        return TRANSPOSITION_ERR_TUNING_BEHAVIOR_NOT_SUPPORTED;
    }
    if (has_range_policy) {
        return TRANSPOSITION_ERR_RANGE_POLICY_NOT_SUPPORTED;
    }
    if (has_instrument_context) {
        return TRANSPOSITION_ERR_INSTRUMENT_CONTEXT_NOT_SUPPORTED;
    }
    if (provenance == NULL) {
        return TRANSPOSITION_ERR_NULL_PROVENANCE;
    }

    Transposition *transposition = (Transposition *)malloc(sizeof(Transposition));
    if (transposition == NULL) {
        return TRANSPOSITION_ERR_ALLOC;
    }

    transposition->spelling_behavior = copy_string(spelling_behavior);
    if (transposition->spelling_behavior == NULL) {
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    Value *parameters;
    if (value_create("semitones", &semitones, sizeof(semitones), &parameters) != VALUE_OK) {
        free(transposition->spelling_behavior);
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", parameters, "NoteEvent", "rhythm, event order, articulation", "pitch",
        "one-to-one, per-event pitch displacement",
        "spelling behavior applied per configured policy; tuning/range/instrument checks not yet implemented "
        "(Deliverable D)",
        true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        value_destroy(parameters);
        free(transposition->spelling_behavior);
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    transposition->transformation = transformation;
    transposition->semitones = semitones;
    transposition->has_tuning_behavior = has_tuning_behavior;
    transposition->has_range_policy = has_range_policy;
    transposition->has_instrument_context = has_instrument_context;

    *out = transposition;
    return TRANSPOSITION_OK;
}

void transposition_destroy(Transposition *transposition) {
    if (transposition == NULL) {
        return;
    }
    transformation_destroy(transposition->transformation);
    free(transposition->spelling_behavior);
    free(transposition);
}

const EntityID *transposition_id(const Transposition *transposition) {
    return transformation_id(transposition->transformation);
}

const Transformation *transposition_transformation(const Transposition *transposition) {
    return transposition->transformation;
}

long transposition_semitones(const Transposition *transposition) {
    return transposition->semitones;
}

const char *transposition_spelling_behavior(const Transposition *transposition) {
    return transposition->spelling_behavior;
}

bool transposition_has_tuning_behavior(const Transposition *transposition) {
    return transposition->has_tuning_behavior;
}

bool transposition_has_range_policy(const Transposition *transposition) {
    return transposition->has_range_policy;
}

bool transposition_has_instrument_context(const Transposition *transposition) {
    return transposition->has_instrument_context;
}

bool transposition_identity_equals(const Transposition *a, const Transposition *b) {
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

TranspositionApplyResult transposition_apply(const Transposition *transposition, const NoteEvent *event,
                                              const PitchNumericValue *event_pitch_value, TransformResult **out) {
    if (transposition == NULL) {
        return TRANSPOSITION_APPLY_ERR_NULL_TRANSPOSITION;
    }
    if (event == NULL) {
        return TRANSPOSITION_APPLY_ERR_NULL_EVENT;
    }
    if (event_pitch_value == NULL) {
        return TRANSPOSITION_APPLY_ERR_NULL_PITCH_VALUE;
    }

    PitchNumericValue *shifted = NULL;
    if (pitch_numeric_value_add_interval(event_pitch_value, transposition->semitones, 1, &shifted) !=
        PITCH_NUMERIC_VALUE_OP_OK) {
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }

    PitchIdentity *new_pitch = NULL;
    PitchNumericValueToIdentityResult identity_result = pitch_numeric_value_to_identity(shifted, &new_pitch);
    pitch_numeric_value_destroy(shifted);
    if (identity_result != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK) {
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }

    NoteEvent *derived = clone_note_event_with_new_pitch(event, new_pitch, "-transposed");
    if (derived == NULL) {
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }
    return TRANSPOSITION_APPLY_OK;
}