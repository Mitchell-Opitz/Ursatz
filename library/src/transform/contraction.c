#include "transform/contraction.h"

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

struct Contraction {
    Transformation *transformation;
    EntityID *source_structure_id;
    EntityID **removed_event_ids;
    size_t removed_event_id_count;
};

ContractionResult contraction_create(EntityID *id, EntityID *source_structure_id, EntityID **removed_event_ids,
                                      size_t removed_event_id_count, ProvenanceReference *provenance,
                                      Contraction **out) {
    if (id == NULL) {
        return CONTRACTION_ERR_NULL_ID;
    }
    if (source_structure_id == NULL) {
        return CONTRACTION_ERR_NULL_SOURCE_STRUCTURE_ID;
    }
    if (removed_event_ids == NULL || removed_event_id_count == 0) {
        return CONTRACTION_ERR_NULL_REMOVED_EVENT_IDS;
    }
    for (size_t i = 0; i < removed_event_id_count; i++) {
        if (removed_event_ids[i] == NULL) {
            return CONTRACTION_ERR_NULL_REMOVED_EVENT_ID;
        }
    }
    if (provenance == NULL) {
        return CONTRACTION_ERR_NULL_PROVENANCE;
    }

    Contraction *contraction = (Contraction *)malloc(sizeof(Contraction));
    if (contraction == NULL) {
        return CONTRACTION_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * removed_event_id_count);
    if (owned_ids == NULL) {
        free(contraction);
        return CONTRACTION_ERR_ALLOC;
    }
    for (size_t i = 0; i < removed_event_id_count; i++) {
        owned_ids[i] = removed_event_ids[i];
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "Structure", NULL, "Structure", "pitch, rhythm, articulation of the remaining events",
        "event membership (compression)",
        "one-to-one: remaining event references retained by reference, removed event references dropped, never "
        "by duplicating NoteEvent payloads for the ones kept",
        "removed_event_ids must be non-empty and reference events that exist in source_structure_id", true,
        provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(owned_ids);
        free(contraction);
        return CONTRACTION_ERR_ALLOC;
    }

    contraction->transformation = transformation;
    contraction->source_structure_id = source_structure_id;
    contraction->removed_event_ids = owned_ids;
    contraction->removed_event_id_count = removed_event_id_count;

    *out = contraction;
    return CONTRACTION_OK;
}

void contraction_destroy(Contraction *contraction) {
    if (contraction == NULL) {
        return;
    }
    transformation_destroy(contraction->transformation);
    entity_id_destroy(contraction->source_structure_id);
    for (size_t i = 0; i < contraction->removed_event_id_count; i++) {
        entity_id_destroy(contraction->removed_event_ids[i]);
    }
    free(contraction->removed_event_ids);
    free(contraction);
}

const EntityID *contraction_id(const Contraction *contraction) {
    return transformation_id(contraction->transformation);
}

const Transformation *contraction_transformation(const Contraction *contraction) {
    return contraction->transformation;
}

const EntityID *contraction_source_structure_id(const Contraction *contraction) {
    return contraction->source_structure_id;
}

size_t contraction_removed_event_id_count(const Contraction *contraction) {
    return contraction->removed_event_id_count;
}

const EntityID *contraction_removed_event_id_at(const Contraction *contraction, size_t index) {
    return contraction->removed_event_ids[index];
}

bool contraction_identity_equals(const Contraction *a, const Contraction *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

ContractionApplyResult contraction_apply(const Contraction *contraction, NoteEvent **source_events,
                                          size_t source_event_count, TransformResult **out) {
    if (contraction == NULL) {
        return CONTRACTION_APPLY_ERR_NULL_CONTRACTION;
    }
    if (source_events == NULL || source_event_count == 0) {
        return CONTRACTION_APPLY_ERR_NULL_SOURCE_EVENTS;
    }

    for (size_t i = 0; i < contraction->removed_event_id_count; i++) {
        bool found = false;
        for (size_t j = 0; j < source_event_count; j++) {
            if (entity_id_equals(note_event_id(source_events[j]), contraction->removed_event_ids[i])) {
                found = true;
                break;
            }
        }
        if (!found) {
            return CONTRACTION_APPLY_ERR_EVENT_NOT_FOUND;
        }
    }

    NoteEvent **derived = (NoteEvent **)malloc(sizeof(NoteEvent *) * source_event_count);
    if (derived == NULL) {
        return CONTRACTION_APPLY_ERR_ALLOC;
    }

    size_t out_count = 0;
    for (size_t i = 0; i < source_event_count; i++) {
        bool removed = false;
        for (size_t j = 0; j < contraction->removed_event_id_count; j++) {
            if (entity_id_equals(note_event_id(source_events[i]), contraction->removed_event_ids[j])) {
                removed = true;
                break;
            }
        }
        if (removed) {
            continue;
        }

        derived[out_count] = transform_support_clone_note_event(source_events[i], "-contracted");
        if (derived[out_count] == NULL) {
            for (size_t k = 0; k < out_count; k++) {
                note_event_destroy(derived[k]);
            }
            free(derived);
            return CONTRACTION_APPLY_ERR_ALLOC;
        }
        out_count++;
    }

    TransformResultResult result = transform_result_create(out_count > 0 ? derived : NULL, out_count, out);
    if (result != TRANSFORM_RESULT_OK) {
        for (size_t k = 0; k < out_count; k++) {
            note_event_destroy(derived[k]);
        }
        free(derived);
        return CONTRACTION_APPLY_ERR_ALLOC;
    }
    free(derived);
    return CONTRACTION_APPLY_OK;
}