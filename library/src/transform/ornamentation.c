#include "transform/ornamentation.h"

#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "pitch/pitch_spelling.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/transform_support.h"

struct Ornamentation {
    Transformation *transformation;
    EntityID *source_event_id;
    EntityID **derived_event_ids;
    ProvenanceReference **derived_event_provenances;
    size_t derived_event_count;
};

OrnamentationResult ornamentation_create(EntityID *id, EntityID *source_event_id, EntityID **derived_event_ids,
                                          ProvenanceReference **derived_event_provenances, size_t derived_event_count,
                                          ProvenanceReference *provenance, Ornamentation **out) {
    if (id == NULL) {
        return ORNAMENTATION_ERR_NULL_ID;
    }
    if (source_event_id == NULL) {
        return ORNAMENTATION_ERR_NULL_SOURCE_EVENT_ID;
    }
    if (derived_event_ids == NULL || derived_event_count == 0) {
        return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_IDS;
    }
    if (derived_event_provenances == NULL) {
        return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_PROVENANCES;
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        if (derived_event_ids[i] == NULL) {
            return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_ID;
        }
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        ProvenanceReference *entry = derived_event_provenances[i];
        if (entry == NULL) {
            return ORNAMENTATION_ERR_NULL_DERIVED_EVENT_PROVENANCE;
        }
        if (provenance_reference_state(entry) != PROVENANCE_REFERENCE_PRESENT) {
            return ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_ABSENT;
        }
        if (!entity_id_equals(provenance_reference_target_id(entry), source_event_id)) {
            return ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_TARGET_MISMATCH;
        }
    }
    if (provenance == NULL) {
        return ORNAMENTATION_ERR_NULL_PROVENANCE;
    }

    Ornamentation *ornamentation = (Ornamentation *)malloc(sizeof(Ornamentation));
    if (ornamentation == NULL) {
        return ORNAMENTATION_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * derived_event_count);
    if (owned_ids == NULL) {
        free(ornamentation);
        return ORNAMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        owned_ids[i] = derived_event_ids[i];
    }

    ProvenanceReference **owned_provenances =
        (ProvenanceReference **)malloc(sizeof(ProvenanceReference *) * derived_event_count);
    if (owned_provenances == NULL) {
        free(owned_ids);
        free(ornamentation);
        return ORNAMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < derived_event_count; i++) {
        owned_provenances[i] = derived_event_provenances[i];
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "overall harmonic/structural identity of the source event",
        "event count (one-to-many fan-out), ornamental pitch/rhythm detail added",
        "one-to-many: the source event derives multiple decorative events, each carrying an explicit "
        "ProvenanceReference back to the source",
        "every derived event MUST carry a PRESENT ProvenanceReference targeting source_event_id", true, provenance,
        &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(owned_provenances);
        free(owned_ids);
        free(ornamentation);
        return ORNAMENTATION_ERR_ALLOC;
    }

    ornamentation->transformation = transformation;
    ornamentation->source_event_id = source_event_id;
    ornamentation->derived_event_ids = owned_ids;
    ornamentation->derived_event_provenances = owned_provenances;
    ornamentation->derived_event_count = derived_event_count;

    *out = ornamentation;
    return ORNAMENTATION_OK;
}

void ornamentation_destroy(Ornamentation *ornamentation) {
    if (ornamentation == NULL) {
        return;
    }
    transformation_destroy(ornamentation->transformation);
    entity_id_destroy(ornamentation->source_event_id);
    for (size_t i = 0; i < ornamentation->derived_event_count; i++) {
        entity_id_destroy(ornamentation->derived_event_ids[i]);
        provenance_reference_destroy(ornamentation->derived_event_provenances[i]);
    }
    free(ornamentation->derived_event_ids);
    free(ornamentation->derived_event_provenances);
    free(ornamentation);
}

const EntityID *ornamentation_id(const Ornamentation *ornamentation) {
    return transformation_id(ornamentation->transformation);
}

const Transformation *ornamentation_transformation(const Ornamentation *ornamentation) {
    return ornamentation->transformation;
}

const EntityID *ornamentation_source_event_id(const Ornamentation *ornamentation) {
    return ornamentation->source_event_id;
}

size_t ornamentation_derived_event_count(const Ornamentation *ornamentation) {
    return ornamentation->derived_event_count;
}

const EntityID *ornamentation_derived_event_id_at(const Ornamentation *ornamentation, size_t index) {
    return ornamentation->derived_event_ids[index];
}

const ProvenanceReference *ornamentation_derived_event_provenance_at(const Ornamentation *ornamentation,
                                                                      size_t index) {
    return ornamentation->derived_event_provenances[index];
}

bool ornamentation_identity_equals(const Ornamentation *a, const Ornamentation *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

static EntityID *copy_entity_id(const EntityID *source) {
    EntityID *copy = NULL;
    if (entity_id_create(entity_id_value(source), &copy) != ENTITY_ID_OK) {
        return NULL;
    }
    return copy;
}

static ProvenanceReference *copy_provenance_reference(const ProvenanceReference *source) {
    if (provenance_reference_state(source) == PROVENANCE_REFERENCE_ABSENT) {
        ProvenanceReference *copy = NULL;
        if (provenance_reference_create_absent(&copy) != PROVENANCE_REFERENCE_OK) {
            return NULL;
        }
        return copy;
    }
    EntityID *target_copy = copy_entity_id(provenance_reference_target_id(source));
    if (target_copy == NULL) {
        return NULL;
    }
    ProvenanceReference *copy = NULL;
    if (provenance_reference_create_present(target_copy, &copy) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(target_copy);
        return NULL;
    }
    return copy;
}

/* Builds one derived NoteEvent for spec, using id/provenance already
   declared (and validated) by ornamentation_create at index i, rather
   than minting new ones. Returns NULL on any allocation failure. */
static NoteEvent *build_derived_event(const Ornamentation *ornamentation, const NoteEvent *source_event,
                                       const OrnamentEventSpec *spec, size_t index) {
    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(source_event));
    MusicalTime *new_position = NULL;
    if (musical_time_add(source_position, spec->offset, &new_position) != MUSICAL_TIME_OK) {
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(new_position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(new_position);
        return NULL;
    }

    Duration *duration = NULL;
    if (duration_create(duration_numerator(spec->duration), duration_denominator(spec->duration), &duration) !=
        DURATION_OK) {
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchIdentity *pitch = transform_support_clone_pitch_identity(spec->pitch);
    if (pitch == NULL) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!transform_support_clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source_event), &spelling)) {
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!transform_support_clone_voice_reference_or_null(note_event_optional_voice(source_event), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    EntityID *id = copy_entity_id(ornamentation->derived_event_ids[index]);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    ProvenanceReference *provenance = copy_provenance_reference(ornamentation->derived_event_provenances[index]);
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

OrnamentationApplyResult ornamentation_apply(const Ornamentation *ornamentation, const NoteEvent *source_event,
                                              const OrnamentEventSpec *specs, size_t spec_count,
                                              TransformResult **out) {
    if (ornamentation == NULL) {
        return ORNAMENTATION_APPLY_ERR_NULL_ORNAMENTATION;
    }
    if (source_event == NULL) {
        return ORNAMENTATION_APPLY_ERR_NULL_SOURCE_EVENT;
    }
    if (specs == NULL) {
        return ORNAMENTATION_APPLY_ERR_NULL_SPECS;
    }
    if (spec_count != ornamentation->derived_event_count) {
        return ORNAMENTATION_APPLY_ERR_SPEC_COUNT_MISMATCH;
    }
    for (size_t i = 0; i < spec_count; i++) {
        if (specs[i].offset == NULL || specs[i].duration == NULL || specs[i].pitch == NULL) {
            return ORNAMENTATION_APPLY_ERR_NULL_SPEC_FIELD;
        }
    }

    NoteEvent **derived = (NoteEvent **)malloc(sizeof(NoteEvent *) * spec_count);
    if (derived == NULL) {
        return ORNAMENTATION_APPLY_ERR_ALLOC;
    }

    for (size_t i = 0; i < spec_count; i++) {
        derived[i] = build_derived_event(ornamentation, source_event, &specs[i], i);
        if (derived[i] == NULL) {
            for (size_t k = 0; k < i; k++) {
                note_event_destroy(derived[k]);
            }
            free(derived);
            return ORNAMENTATION_APPLY_ERR_ALLOC;
        }
    }

    TransformResultResult result = transform_result_create(derived, spec_count, out);
    if (result != TRANSFORM_RESULT_OK) {
        for (size_t k = 0; k < spec_count; k++) {
            note_event_destroy(derived[k]);
        }
        free(derived);
        return ORNAMENTATION_APPLY_ERR_ALLOC;
    }
    free(derived);
    return ORNAMENTATION_APPLY_OK;
}