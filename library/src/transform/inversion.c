#include "transform/inversion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

struct Inversion {
    Transformation *transformation;
    PitchIdentity *axis;
    char *pitch_space;
    char *spelling_policy;
    bool has_tuning_semantics;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

InversionResult inversion_create(EntityID *id, PitchIdentity *axis, const char *pitch_space,
                                  const char *spelling_policy, bool has_tuning_semantics,
                                  ProvenanceReference *provenance, Inversion **out) {
    if (id == NULL) {
        return INVERSION_ERR_NULL_ID;
    }
    if (axis == NULL) {
        return INVERSION_ERR_NULL_AXIS;
    }
    if (pitch_space == NULL || pitch_space[0] == '\0') {
        return INVERSION_ERR_NULL_PITCH_SPACE;
    }
    if (spelling_policy == NULL || spelling_policy[0] == '\0') {
        return INVERSION_ERR_NULL_SPELLING_POLICY;
    }
    if (has_tuning_semantics) {
        return INVERSION_ERR_TUNING_SEMANTICS_NOT_SUPPORTED;
    }
    if (provenance == NULL) {
        return INVERSION_ERR_NULL_PROVENANCE;
    }

    Inversion *inversion = (Inversion *)malloc(sizeof(Inversion));
    if (inversion == NULL) {
        return INVERSION_ERR_ALLOC;
    }

    inversion->pitch_space = copy_string(pitch_space);
    if (inversion->pitch_space == NULL) {
        free(inversion);
        return INVERSION_ERR_ALLOC;
    }

    inversion->spelling_policy = copy_string(spelling_policy);
    if (inversion->spelling_policy == NULL) {
        free(inversion->pitch_space);
        free(inversion);
        return INVERSION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result =
        transformation_create(id, "NoteEvent", NULL, "NoteEvent", "rhythm, event order, articulation", "pitch",
                               "one-to-one, per-event pitch reflection about axis",
                               "spelling policy applied per configured policy; tuning semantics not yet implemented "
                               "(Deliverable D)",
                               true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(inversion->spelling_policy);
        free(inversion->pitch_space);
        free(inversion);
        return INVERSION_ERR_ALLOC;
    }

    inversion->transformation = transformation;
    inversion->axis = axis;
    inversion->has_tuning_semantics = has_tuning_semantics;

    *out = inversion;
    return INVERSION_OK;
}

void inversion_destroy(Inversion *inversion) {
    if (inversion == NULL) {
        return;
    }
    transformation_destroy(inversion->transformation);
    pitch_identity_destroy(inversion->axis);
    free(inversion->pitch_space);
    free(inversion->spelling_policy);
    free(inversion);
}

const EntityID *inversion_id(const Inversion *inversion) {
    return transformation_id(inversion->transformation);
}

const Transformation *inversion_transformation(const Inversion *inversion) {
    return inversion->transformation;
}

const PitchIdentity *inversion_axis(const Inversion *inversion) {
    return inversion->axis;
}

const char *inversion_pitch_space(const Inversion *inversion) {
    return inversion->pitch_space;
}

const char *inversion_spelling_policy(const Inversion *inversion) {
    return inversion->spelling_policy;
}

bool inversion_has_tuning_semantics(const Inversion *inversion) {
    return inversion->has_tuning_semantics;
}

bool inversion_identity_equals(const Inversion *a, const Inversion *b) {
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

InversionApplyResult inversion_apply(const Inversion *inversion, const NoteEvent *event,
                                      const PitchNumericValue *event_pitch_value,
                                      const PitchNumericValue *axis_value, TransformResult **out) {
    if (inversion == NULL) {
        return INVERSION_APPLY_ERR_NULL_INVERSION;
    }
    if (event == NULL) {
        return INVERSION_APPLY_ERR_NULL_EVENT;
    }
    if (event_pitch_value == NULL) {
        return INVERSION_APPLY_ERR_NULL_PITCH_VALUE;
    }
    if (axis_value == NULL) {
        return INVERSION_APPLY_ERR_NULL_AXIS_VALUE;
    }

    PitchNumericValue *reflected = NULL;
    PitchNumericValueOpResult reflect_result =
        pitch_numeric_value_reflect_about_axis(event_pitch_value, axis_value, &reflected);
    if (reflect_result == PITCH_NUMERIC_VALUE_OP_ERR_TUNING_SYSTEM_MISMATCH) {
        return INVERSION_APPLY_ERR_TUNING_SYSTEM_MISMATCH;
    }
    if (reflect_result != PITCH_NUMERIC_VALUE_OP_OK) {
        return INVERSION_APPLY_ERR_ALLOC;
    }

    PitchIdentity *new_pitch = NULL;
    PitchNumericValueToIdentityResult identity_result = pitch_numeric_value_to_identity(reflected, &new_pitch);
    pitch_numeric_value_destroy(reflected);
    if (identity_result != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK) {
        return INVERSION_APPLY_ERR_ALLOC;
    }

    NoteEvent *derived = clone_note_event_with_new_pitch(event, new_pitch, "-inverted");
    if (derived == NULL) {
        return INVERSION_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return INVERSION_APPLY_ERR_ALLOC;
    }
    return INVERSION_APPLY_OK;
}