#include "structure/structure.h"

#include <stdlib.h>

struct Structure {
    Entity *entity;
    EntityID **event_ids;
    size_t event_id_count;
};

StructureResult structure_create(Entity *entity, EntityID **event_ids, size_t event_id_count, Structure **out) {
    if (entity == NULL) {
        return STRUCTURE_ERR_NULL_ENTITY;
    }
    if (event_ids == NULL || event_id_count == 0) {
        return STRUCTURE_ERR_NULL_EVENT_IDS;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        if (event_ids[i] == NULL) {
            return STRUCTURE_ERR_NULL_EVENT_ID;
        }
    }

    Structure *structure = (Structure *)malloc(sizeof(Structure));
    if (structure == NULL) {
        return STRUCTURE_ERR_ALLOC;
    }

    EntityID **owned_ids = (EntityID **)malloc(sizeof(EntityID *) * event_id_count);
    if (owned_ids == NULL) {
        free(structure);
        return STRUCTURE_ERR_ALLOC;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        owned_ids[i] = event_ids[i];
    }

    structure->entity = entity;
    structure->event_ids = owned_ids;
    structure->event_id_count = event_id_count;

    *out = structure;
    return STRUCTURE_OK;
}

void structure_destroy(Structure *structure) {
    if (structure == NULL) {
        return;
    }
    entity_destroy(structure->entity);
    for (size_t i = 0; i < structure->event_id_count; i++) {
        entity_id_destroy(structure->event_ids[i]);
    }
    free(structure->event_ids);
    free(structure);
}

const Entity *structure_entity(const Structure *structure) {
    return structure->entity;
}

size_t structure_event_id_count(const Structure *structure) {
    return structure->event_id_count;
}

const EntityID *structure_event_id_at(const Structure *structure, size_t index) {
    return structure->event_ids[index];
}