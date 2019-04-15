#include "transform/fragmentation.h"

#include <stdlib.h>

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