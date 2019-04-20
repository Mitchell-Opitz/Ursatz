#include "transform/contraction.h"

#include <stdlib.h>

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