#include "transform/expansion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

struct Expansion {
    Transformation *transformation;
    EntityID *source_structure_id;
    EntityID **appended_event_ids;
    size_t appended_event_id_count;
};

ExpansionResult expansion_create(EntityID *id, EntityID *source_structure_id, EntityID **appended_event_ids,
                                  size_t appended_event_id_count, ProvenanceReference *provenance,
                                  Expansion **out) {
    if (id == NULL) {
        return EXPANSION_ERR_NULL_ID;
    }
    if (source_structure_id == NULL) {
        return EXPANSION_ERR_NULL_SOURCE_STRUCTURE_ID;
    }
    if (appended_event_ids == NULL || appended_event_id_count == 0) {
        return EXPANSION_ERR_NULL_APPENDED_EVENT_IDS;
    }
    for (size_t i = 0; i < appended_event_id_count; i++) {
        if (appended_event_ids[i] == NULL) {
            return EXPANSION_ERR_NULL_APPENDED_EVENT_ID;
        }
    }
    if (provenance == NULL) {
        return EXPANSION_ERR_NULL_PROVENANCE;
    }

    Expansion *expansion = (Expansion *)malloc(sizeof(Expansion));
    if (expansion == NULL) {
        return EXPANSION_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * appended_event_id_count);
    if (owned_ids == NULL) {
        free(expansion);
        return EXPANSION_ERR_ALLOC;
    }
    for (size_t i = 0; i < appended_event_id_count; i++) {
        owned_ids[i] = appended_event_ids[i];
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "Structure", NULL, "Structure", "pitch, rhythm, articulation of the original events",
        "event membership (extension)",
        "one-to-many: original event references retained, appended event references added by reference, never "
        "by duplicating NoteEvent payloads",
        "appended_event_ids must be non-empty and reference events that exist", true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(owned_ids);
        free(expansion);
        return EXPANSION_ERR_ALLOC;
    }

    expansion->transformation = transformation;
    expansion->source_structure_id = source_structure_id;
    expansion->appended_event_ids = owned_ids;
    expansion->appended_event_id_count = appended_event_id_count;

    *out = expansion;
    return EXPANSION_OK;
}

void expansion_destroy(Expansion *expansion) {
    if (expansion == NULL) {
        return;
    }
    transformation_destroy(expansion->transformation);
    entity_id_destroy(expansion->source_structure_id);
    for (size_t i = 0; i < expansion->appended_event_id_count; i++) {
        entity_id_destroy(expansion->appended_event_ids[i]);
    }
    free(expansion->appended_event_ids);
    free(expansion);
}

const EntityID *expansion_id(const Expansion *expansion) {
    return transformation_id(expansion->transformation);
}

const Transformation *expansion_transformation(const Expansion *expansion) {
    return expansion->transformation;
}

const EntityID *expansion_source_structure_id(const Expansion *expansion) {
    return expansion->source_structure_id;
}

size_t expansion_appended_event_id_count(const Expansion *expansion) {
    return expansion->appended_event_id_count;
}

const EntityID *expansion_appended_event_id_at(const Expansion *expansion, size_t index) {
    return expansion->appended_event_ids[index];
}

bool expansion_identity_equals(const Expansion *a, const Expansion *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
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

/* Builds a new NoteEvent identical to source (fresh copies of every
   field) except a new id/provenance. Returns NULL on any allocation
   failure. */
static NoteEvent *clone_note_event(const NoteEvent *source, const char *id_suffix) {
    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(source));
    MusicalTime *position = NULL;
    if (musical_time_create(musical_time_numerator(source_position), musical_time_denominator(source_position),
                             &position) != MUSICAL_TIME_OK) {
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

static void destroy_partial(NoteEvent **derived, size_t count) {
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(derived[i]);
    }
    free(derived);
}

ExpansionApplyResult expansion_apply(const Expansion *expansion, NoteEvent **source_events,
                                      size_t source_event_count, NoteEvent **appended_events,
                                      size_t appended_event_count, size_t position, TransformResult **out) {
    if (expansion == NULL) {
        return EXPANSION_APPLY_ERR_NULL_EXPANSION;
    }
    if (source_events == NULL && source_event_count > 0) {
        return EXPANSION_APPLY_ERR_NULL_SOURCE_EVENTS;
    }
    if (appended_events == NULL) {
        return EXPANSION_APPLY_ERR_NULL_APPENDED_EVENTS;
    }
    if (appended_event_count != expansion->appended_event_id_count) {
        return EXPANSION_APPLY_ERR_APPENDED_EVENT_COUNT_MISMATCH;
    }
    for (size_t i = 0; i < appended_event_count; i++) {
        if (!entity_id_equals(note_event_id(appended_events[i]), expansion->appended_event_ids[i])) {
            return EXPANSION_APPLY_ERR_APPENDED_EVENT_ID_MISMATCH;
        }
    }
    if (position > source_event_count) {
        return EXPANSION_APPLY_ERR_POSITION_OUT_OF_RANGE;
    }

    size_t total = source_event_count + appended_event_count;
    NoteEvent **derived = (NoteEvent **)malloc(sizeof(NoteEvent *) * total);
    if (derived == NULL) {
        return EXPANSION_APPLY_ERR_ALLOC;
    }

    size_t out_index = 0;
    for (size_t i = 0; i < position; i++) {
        derived[out_index] = clone_note_event(source_events[i], "-expanded");
        if (derived[out_index] == NULL) {
            destroy_partial(derived, out_index);
            return EXPANSION_APPLY_ERR_ALLOC;
        }
        out_index++;
    }
    for (size_t i = 0; i < appended_event_count; i++) {
        derived[out_index] = clone_note_event(appended_events[i], "-expanded");
        if (derived[out_index] == NULL) {
            destroy_partial(derived, out_index);
            return EXPANSION_APPLY_ERR_ALLOC;
        }
        out_index++;
    }
    for (size_t i = position; i < source_event_count; i++) {
        derived[out_index] = clone_note_event(source_events[i], "-expanded");
        if (derived[out_index] == NULL) {
            destroy_partial(derived, out_index);
            return EXPANSION_APPLY_ERR_ALLOC;
        }
        out_index++;
    }

    TransformResultResult result = transform_result_create(derived, total, out);
    if (result != TRANSFORM_RESULT_OK) {
        destroy_partial(derived, total);
        return EXPANSION_APPLY_ERR_ALLOC;
    }
    free(derived);
    return EXPANSION_APPLY_OK;
}