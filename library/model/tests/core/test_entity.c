#include "model/core/entity.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static Entity *make_entity(const char *id_value, unsigned long schema_value, const char *type) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    SchemaVersion *version = NULL;
    schema_version_create(schema_value, &version);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    Entity *entity = NULL;
    entity_create(id, type, version, NULL, provenance, &entity);
    return entity;
}

static void test_create_rejects_null_id(void) {
    SchemaVersion *version = NULL;
    schema_version_create(1, &version);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    Entity *entity = NULL;
    EntityResult r = entity_create(NULL, "Work", version, NULL, provenance, &entity);
    assert(r == ENTITY_ERR_NULL_ID);
    assert(entity == NULL);

    schema_version_destroy(version);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_schema_version(void) {
    EntityID *id = NULL;
    entity_id_create("work-1", &id);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    Entity *entity = NULL;
    EntityResult r = entity_create(id, "Work", NULL, NULL, provenance, &entity);
    assert(r == ENTITY_ERR_NULL_SCHEMA_VERSION);
    assert(entity == NULL);

    entity_id_destroy(id);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_type(void) {
    EntityID *id = NULL;
    entity_id_create("work-1", &id);

    SchemaVersion *version = NULL;
    schema_version_create(1, &version);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    Entity *entity = NULL;
    EntityResult r = entity_create(id, NULL, version, NULL, provenance, &entity);
    assert(r == ENTITY_ERR_NULL_TYPE);
    assert(entity == NULL);

    entity_id_destroy(id);
    schema_version_destroy(version);
    provenance_reference_destroy(provenance);
}

static void test_create_rejects_null_provenance(void) {
    EntityID *id = NULL;
    entity_id_create("work-1", &id);

    SchemaVersion *version = NULL;
    schema_version_create(1, &version);

    Entity *entity = NULL;
    EntityResult r = entity_create(id, "Work", version, NULL, NULL, &entity);
    assert(r == ENTITY_ERR_NULL_PROVENANCE);
    assert(entity == NULL);

    entity_id_destroy(id);
    schema_version_destroy(version);
}

static void test_create_accepts_valid(void) {
    EntityID *id = NULL;
    SchemaVersion *version = NULL;
    ProvenanceReference *provenance = NULL;
    Entity *entity = NULL;
    assert(entity_id_create("work-1", &id) == ENTITY_ID_OK);
    assert(schema_version_create(1, &version) == SCHEMA_VERSION_OK);
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(entity_create(id, "Work", version, NULL, provenance, &entity) == ENTITY_OK);
    assert(entity != NULL);
    /* entity.h reads back the id only (the type and the schema version have no accessor in it): the id is the one given. */
    assert(strcmp(entity_id_value(entity_id(entity)), "work-1") == 0);
    entity_destroy(entity);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_schema_version();
    test_create_rejects_null_type();
    test_create_rejects_null_provenance();
    test_create_accepts_valid();
    return 0;
}