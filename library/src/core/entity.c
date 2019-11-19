#include "core/entity.h"

#include <stdlib.h>
#include <string.h>

struct Entity {
    EntityID *id;
    char *type;
    SchemaVersion *schema_version;
    char *metadata;
    ProvenanceReference *provenance_reference;
};

EntityResult entity_create(EntityID *id, const char *type, SchemaVersion *schema_version,
                            const char *metadata, ProvenanceReference *provenance, Entity **out) {
    if (id == NULL) {
        return ENTITY_ERR_NULL_ID;
    }
    if (type == NULL || type[0] == '\0') {
        return ENTITY_ERR_NULL_TYPE;
    }
    if (schema_version == NULL) {
        return ENTITY_ERR_NULL_SCHEMA_VERSION;
    }
    if (provenance == NULL) {
        return ENTITY_ERR_NULL_PROVENANCE;
    }

    Entity *entity = (Entity *)malloc(sizeof(Entity));
    if (entity == NULL) {
        return ENTITY_ERR_ALLOC;
    }

    size_t type_len = strlen(type);
    entity->type = (char *)malloc(type_len + 1);
    if (entity->type == NULL) {
        free(entity);
        return ENTITY_ERR_ALLOC;
    }
    memcpy(entity->type, type, type_len + 1);

    if (metadata != NULL) {
        size_t metadata_len = strlen(metadata);
        entity->metadata = (char *)malloc(metadata_len + 1);
        if (entity->metadata == NULL) {
            free(entity->type);
            free(entity);
            return ENTITY_ERR_ALLOC;
        }
        memcpy(entity->metadata, metadata, metadata_len + 1);
    } else {
        entity->metadata = NULL;
    }

    entity->id = id;
    entity->schema_version = schema_version;
    entity->provenance_reference = provenance;

    *out = entity;
    return ENTITY_OK;
}

void entity_destroy(Entity *entity) {
    if (entity == NULL) {
        return;
    }
    entity_id_destroy(entity->id);
    schema_version_destroy(entity->schema_version);
    free(entity->type);
    free(entity->metadata);
    provenance_reference_destroy(entity->provenance_reference);
    free(entity);
}

const EntityID *entity_id(const Entity *entity) {
    return entity->id;
}

const char *entity_type(const Entity *entity) {
    return entity->type;
}

const SchemaVersion *entity_schema_version(const Entity *entity) {
    return entity->schema_version;
}

const char *entity_metadata(const Entity *entity) {
    return entity->metadata;
}

const ProvenanceReference *entity_provenance_reference(const Entity *entity) {
    return entity->provenance_reference;
}

bool entity_identity_equals(const Entity *a, const Entity *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id) && schema_version_equals(a->schema_version, b->schema_version);
}