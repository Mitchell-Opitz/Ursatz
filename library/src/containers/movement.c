#include "containers/movement.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Movement {
    EntityID *id;
    EntityID **section_ids;
    size_t section_id_count;
};

MovementResult movement_create(EntityID *id, EntityID **section_ids, size_t section_id_count, Movement **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(id, section_ids, section_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return MOVEMENT_ERR_NULL_ID;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return MOVEMENT_ERR_NULL_SECTION_ID;
        default:
            return MOVEMENT_ERR_ALLOC;
    }

    Movement *movement = (Movement *)malloc(sizeof(Movement));
    if (movement == NULL) {
        free(copy);
        return MOVEMENT_ERR_ALLOC;
    }

    movement->section_ids = copy;
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
    id_array_destroy(movement->section_ids, movement->section_id_count);
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