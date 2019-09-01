#include "containers/movement.h"

#include <stdlib.h>

struct Movement {
    EntityID *id;
    EntityID **section_ids;
    size_t section_id_count;
};

static EntityID **copy_id_array(EntityID **ids, size_t count) {
    if (count == 0) {
        return NULL;
    }
    EntityID **copy = (EntityID **)malloc(sizeof(EntityID *) * count);
    if (copy == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = ids[i];
    }
    return copy;
}

static void destroy_id_array(EntityID **ids, size_t count) {
    if (ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(ids[i]);
    }
    free(ids);
}

MovementResult movement_create(EntityID *id, EntityID **section_ids, size_t section_id_count, Movement **out) {
    if (id == NULL) {
        return MOVEMENT_ERR_NULL_ID;
    }
    for (size_t i = 0; i < section_id_count; i++) {
        if (section_ids[i] == NULL) {
            return MOVEMENT_ERR_NULL_SECTION_ID;
        }
    }

    Movement *movement = (Movement *)malloc(sizeof(Movement));
    if (movement == NULL) {
        return MOVEMENT_ERR_ALLOC;
    }

    movement->section_ids = copy_id_array(section_ids, section_id_count);
    if (section_id_count > 0 && movement->section_ids == NULL) {
        free(movement);
        return MOVEMENT_ERR_ALLOC;
    }

    movement->id = id;
    movement->section_id_count = section_id_count;

    *out = movement;
    return MOVEMENT_OK;
}

void movement_destroy(Movement *movement) {
    if (movement == NULL) {
        return;
    }
    entity_id_destroy(movement->id);
    destroy_id_array(movement->section_ids, movement->section_id_count);
    free(movement);
}

const EntityID *movement_id(const Movement *movement) {
    return movement->id;
}

size_t movement_section_id_count(const Movement *movement) {
    return movement->section_id_count;
}

const EntityID *movement_section_id_at(const Movement *movement, size_t index) {
    return movement->section_ids[index];
}

bool movement_identity_equals(const Movement *a, const Movement *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}