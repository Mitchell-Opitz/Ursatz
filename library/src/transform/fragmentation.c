#include "transform/fragmentation.h"

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

struct Fragmentation {
    Transformation *transformation;
    EntityID *source_structure_id;
    EntityID **extracted_event_ids;
    size_t extracted_event_id_count;
};

FragmentationResult fragmentation_create(EntityID *id, EntityID *source_structure_id, EntityID **extracted_event_ids,
                                          size_t extracted_event_id_count, ProvenanceReference *provenance,
                                          Fragmentation **out) {
    if (id == NULL) {
        return FRAGMENTATION_ERR_NULL_ID;
    }
    if (source_structure_id == NULL) {
        return FRAGMENTATION_ERR_NULL_SOURCE_STRUCTURE_ID;
    }
    if (extracted_event_ids == NULL || extracted_event_id_count == 0) {
        return FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_IDS;
    }
    for (size_t i = 0; i < extracted_event_id_count; i++) {
        if (extracted_event_ids[i] == NULL) {
            return FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_ID;
        }
    }
    if (provenance == NULL) {
        return FRAGMENTATION_ERR_NULL_PROVENANCE;
    }

    Fragmentation *fragmentation = (Fragmentation *)malloc(sizeof(Fragmentation));
    if (fragmentation == NULL) {
        return FRAGMENTATION_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * extracted_event_id_count);
    if (owned_ids == NULL) {
        free(fragmentation);
        return FRAGMENTATION_ERR_ALLOC;
    }
    for (size_t i = 0; i < extracted_event_id_count; i++) {
        owned_ids[i] = extracted_event_ids[i];
    }

    Transformation *transformation = NULL;
    TransformationResult transformation_result = transformation_create(
        id, "Structure", NULL, "Structure", "pitch, rhythm, articulation of the retained events",
        "event membership (subset extraction)",
        "many-to-one: selected event references copied into a new Structure by reference, never by duplicating "
        "NoteEvent payloads",
        "extracted_event_ids must be non-empty and reference events that exist in source_structure_id", true,
        provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(owned_ids);
        free(fragmentation);
        return FRAGMENTATION_ERR_ALLOC;
    }

    fragmentation->transformation = transformation;
    fragmentation->source_structure_id = source_structure_id;
    fragmentation->extracted_event_ids = owned_ids;
    fragmentation->extracted_event_id_count = extracted_event_id_count;

    *out = fragmentation;
    return FRAGMENTATION_OK;
}

void fragmentation_destroy(Fragmentation *fragmentation) {
    if (fragmentation == NULL) {
        return;
    }
    transformation_destroy(fragmentation->transformation);
    entity_id_destroy(fragmentation->source_structure_id);
    for (size_t i = 0; i < fragmentation->extracted_event_id_count; i++) {
        entity_id_destroy(fragmentation->extracted_event_ids[i]);
    }
    free(fragmentation->extracted_event_ids);
    free(fragmentation);
}

const EntityID *fragmentation_id(const Fragmentation *fragmentation) {
    return transformation_id(fragmentation->transformation);
}

const Transformation *fragmentation_transformation(const Fragmentation *fragmentation) {
    return fragmentation->transformation;
}

const EntityID *fragmentation_source_structure_id(const Fragmentation *fragmentation) {
    return fragmentation->source_structure_id;
}

size_t fragmentation_extracted_event_id_count(const Fragmentation *fragmentation) {
    return fragmentation->extracted_event_id_count;
}

const EntityID *fragmentation_extracted_event_id_at(const Fragmentation *fragmentation, size_t index) {
    return fragmentation->extracted_event_ids[index];
}

bool fragmentation_identity_equals(const Fragmentation *a, const Fragmentation *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

FragmentationApplyResult fragmentation_apply(const Fragmentation *fragmentation, NoteEvent **source_events,
                                              size_t source_event_count, TransformResult **out) {
    if (fragmentation == NULL) {
        return FRAGMENTATION_APPLY_ERR_NULL_FRAGMENTATION;
    }
    if (source_events == NULL || source_event_count == 0) {
        return FRAGMENTATION_APPLY_ERR_NULL_SOURCE_EVENTS;
    }

    NoteEvent **derived = (NoteEvent **)malloc(sizeof(NoteEvent *) * fragmentation->extracted_event_id_count);
    if (derived == NULL) {
        return FRAGMENTATION_APPLY_ERR_ALLOC;
    }

    for (size_t i = 0; i < fragmentation->extracted_event_id_count; i++) {
        const EntityID *target_id = fragmentation->extracted_event_ids[i];
        const NoteEvent *match = NULL;
        for (size_t j = 0; j < source_event_count; j++) {
            if (entity_id_equals(note_event_id(source_events[j]), target_id)) {
                match = source_events[j];
                break;
            }
        }
        if (match == NULL) {
            for (size_t k = 0; k < i; k++) {
                note_event_destroy(derived[k]);
            }
            free(derived);
            return FRAGMENTATION_APPLY_ERR_EVENT_NOT_FOUND;
        }

        derived[i] = transform_support_clone_note_event(match, "-fragment");
        if (derived[i] == NULL) {
            for (size_t k = 0; k < i; k++) {
                note_event_destroy(derived[k]);
            }
            free(derived);
            return FRAGMENTATION_APPLY_ERR_ALLOC;
        }
    }

    TransformResultResult result = transform_result_create(derived, fragmentation->extracted_event_id_count, out);
    if (result != TRANSFORM_RESULT_OK) {
        for (size_t k = 0; k < fragmentation->extracted_event_id_count; k++) {
            note_event_destroy(derived[k]);
        }
        free(derived);
        return FRAGMENTATION_APPLY_ERR_ALLOC;
    }
    free(derived);
    return FRAGMENTATION_APPLY_OK;
}