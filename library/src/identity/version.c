#include "identity/version.h"

#include <stdlib.h>

struct Version {
    EntityID *entity_id;
    unsigned long revision;
};

VersionResult version_create(const EntityID *entity_id, unsigned long revision, Version **out) {
    if (entity_id == NULL) {
        return VERSION_ERR_NULL_ENTITY_ID;
    }
    if (revision == 0) {
        return VERSION_ERR_NON_POSITIVE_REVISION;
    }

    Version *v = (Version *)malloc(sizeof(Version));
    if (v == NULL) {
        return VERSION_ERR_ALLOC;
    }

    EntityID *copy = NULL;
    if (entity_id_create(entity_id_value(entity_id), &copy) != ENTITY_ID_OK) {
        free(v);
        return VERSION_ERR_ALLOC;
    }

    v->entity_id = copy;
    v->revision = revision;

    *out = v;
    return VERSION_OK;
}

void version_destroy(Version *version) {
    if (version == NULL) {
        return;
    }
    entity_id_destroy(version->entity_id);
    free(version);
}

bool version_equals(const Version *a, const Version *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->revision == b->revision && entity_id_equals(a->entity_id, b->entity_id);
}

const EntityID *version_entity_id(const Version *version) {
    return version->entity_id;
}

unsigned long version_revision(const Version *version) {
    return version->revision;
}