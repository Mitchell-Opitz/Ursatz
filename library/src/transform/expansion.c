#include "transform/expansion.h"

#include <stdlib.h>

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