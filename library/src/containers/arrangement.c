#include "containers/arrangement.h"

#include <stdlib.h>

struct Arrangement {
    Entity *entity;
    EntityID *work_id;
};

ArrangementResult arrangement_create(Entity *entity, EntityID *work_id, Arrangement **out) {
    if (entity == NULL) {
        return ARRANGEMENT_ERR_NULL_ENTITY;
    }
    if (work_id == NULL) {
        return ARRANGEMENT_ERR_NULL_WORK_ID;
    }

    Arrangement *arrangement = (Arrangement *)malloc(sizeof(Arrangement));
    if (arrangement == NULL) {
        return ARRANGEMENT_ERR_ALLOC;
    }

    arrangement->entity = entity;
    arrangement->work_id = work_id;

    *out = arrangement;
    return ARRANGEMENT_OK;
}

void arrangement_destroy(Arrangement *arrangement) {
    if (arrangement == NULL) {
        return;
    }
    entity_destroy(arrangement->entity);
    entity_id_destroy(arrangement->work_id);
    free(arrangement);
}

const Entity *arrangement_entity(const Arrangement *arrangement) {
    return arrangement->entity;
}

const EntityID *arrangement_work_id(const Arrangement *arrangement) {
    return arrangement->work_id;
}

bool arrangement_identity_equals(const Arrangement *a, const Arrangement *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_identity_equals(a->entity, b->entity);
}