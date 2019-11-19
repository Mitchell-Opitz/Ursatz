#include "containers/work.h"

#include <stdlib.h>

struct Work {
    Entity *entity;
    EntityID **movement_ids;
    size_t movement_id_count;
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

WorkResult work_create(Entity *entity, EntityID **movement_ids, size_t movement_id_count, Work **out) {
    if (entity == NULL) {
        return WORK_ERR_NULL_ENTITY;
    }
    for (size_t i = 0; i < movement_id_count; i++) {
        if (movement_ids[i] == NULL) {
            return WORK_ERR_NULL_MOVEMENT_ID;
        }
    }

    Work *work = (Work *)malloc(sizeof(Work));
    if (work == NULL) {
        return WORK_ERR_ALLOC;
    }

    work->movement_ids = copy_id_array(movement_ids, movement_id_count);
    if (movement_id_count > 0 && work->movement_ids == NULL) {
        free(work);
        return WORK_ERR_ALLOC;
    }

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
    destroy_id_array(work->movement_ids, work->movement_id_count);
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