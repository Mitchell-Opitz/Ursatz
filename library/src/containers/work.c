#include "containers/work.h"

#include <stdlib.h>

#include "containers/id_array.h"

struct Work {
    Entity *entity;
    EntityID **movement_ids;
    size_t movement_id_count;
};

WorkResult work_create(Entity *entity, EntityID **movement_ids, size_t movement_id_count, Work **out) {
    EntityID **copy = NULL;
    switch (container_shell_create(entity, movement_ids, movement_id_count, &copy)) {
        case CONTAINER_SHELL_OK:
            break;
        case CONTAINER_SHELL_ERR_NULL_OWNER:
            return WORK_ERR_NULL_ENTITY;
        case CONTAINER_SHELL_ERR_NULL_ID:
            return WORK_ERR_NULL_MOVEMENT_ID;
        default:
            return WORK_ERR_ALLOC;
    }

    Work *work = (Work *)malloc(sizeof(Work));
    if (work == NULL) {
        free(copy);
        return WORK_ERR_ALLOC;
    }

    work->movement_ids = copy;
    work->entity = entity;
    work->movement_id_count = movement_id_count;

    *out = work;
    return WORK_OK;
}

void work_destroy(Work *work) {
    if (work == NULL) {
        return;
    }
    entity_destroy(work->entity);
    id_array_destroy(work->movement_ids, work->movement_id_count);
    free(work);
}

const Entity *work_entity(const Work *work) {
    return work->entity;
}

size_t work_movement_id_count(const Work *work) {
    return work->movement_id_count;
}

const EntityID *work_movement_id_at(const Work *work, size_t index) {
    return work->movement_ids[index];
}

bool work_identity_equals(const Work *a, const Work *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_identity_equals(a->entity, b->entity);
}