#include "containers/revision.h"

#include <stdlib.h>

struct Revision {
    Entity *entity;
    EntityID *target_id;
    Version *version;
};

RevisionResult revision_create(Entity *entity, EntityID *target_id, Version *version, Revision **out) {
    if (entity == NULL) {
        return REVISION_ERR_NULL_ENTITY;
    }
    if (target_id == NULL) {
        return REVISION_ERR_NULL_TARGET_ID;
    }
    if (version == NULL) {
        return REVISION_ERR_NULL_VERSION;
    }

    Revision *revision = (Revision *)malloc(sizeof(Revision));
    if (revision == NULL) {
        return REVISION_ERR_ALLOC;
    }

    revision->entity = entity;
    revision->target_id = target_id;
    revision->version = version;

    *out = revision;
    return REVISION_OK;
}

void revision_destroy(Revision *revision) {
    if (revision == NULL) {
        return;
    }
    entity_destroy(revision->entity);
    entity_id_destroy(revision->target_id);
    version_destroy(revision->version);
    free(revision);
}

const Entity *revision_entity(const Revision *revision) {
    return revision->entity;
}

const EntityID *revision_target_id(const Revision *revision) {
    return revision->target_id;
}

const Version *revision_version(const Revision *revision) {
    return revision->version;
}

bool revision_identity_equals(const Revision *a, const Revision *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_identity_equals(a->entity, b->entity);
}