#include "containers/edition.h"

#include <stdlib.h>

struct Edition {
    Entity *entity;
    EntityID *work_id;
};

EditionResult edition_create(Entity *entity, EntityID *work_id, Edition **out) {
    if (entity == NULL) {
        return EDITION_ERR_NULL_ENTITY;
    }
    if (work_id == NULL) {
        return EDITION_ERR_NULL_WORK_ID;
    }

    Edition *edition = (Edition *)malloc(sizeof(Edition));
    if (edition == NULL) {
        return EDITION_ERR_ALLOC;
    }

    edition->entity = entity;
    edition->work_id = work_id;

    *out = edition;
    return EDITION_OK;
}

void edition_destroy(Edition *edition) {
    if (edition == NULL) {
        return;
    }
    entity_destroy(edition->entity);
    entity_id_destroy(edition->work_id);
    free(edition);
}

const Entity *edition_entity(const Edition *edition) {
    return edition->entity;
}

const EntityID *edition_work_id(const Edition *edition) {
    return edition->work_id;
}

bool edition_identity_equals(const Edition *a, const Edition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_identity_equals(a->entity, b->entity);
}