#include "core/entity.h"

#include <assert.h>
#include <stddef.h>

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

static void test_create_accepts_valid(void) {
    Entity *entity = make_entity("work-1", 1, "Work");
    assert(entity != NULL);
    entity_destroy(entity);
}

static void test_identity_equals_same_id_and_version(void) {
    Entity *a = make_entity("work-1", 1, "Work");
    Entity *b = make_entity("work-1", 1, "Work");
    assert(entity_identity_equals(a, b));
    entity_destroy(a);
    entity_destroy(b);
}

static void test_identity_equals_regardless_of_instance(void) {
    Entity *a = make_entity("work-1", 1, "Work");
    Entity *b = make_entity("work-1", 1, "Work");
    assert(a != b);
    assert(entity_identity_equals(a, b));
    entity_destroy(a);
    entity_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Entity *a = make_entity("work-1", 1, "Work");
    Entity *b = make_entity("work-2", 1, "Work");
    assert(!entity_identity_equals(a, b));
    entity_destroy(a);
    entity_destroy(b);
}

static void test_same_field_values_different_id_is_not_identity_equal(void) {
    Entity *a = make_entity("work-1", 1, "Work");
    Entity *b = make_entity("work-2", 1, "Work");
    assert(entity_type(a) != NULL);
    assert(entity_type(b) != NULL);
    assert(!entity_identity_equals(a, b));
    entity_destroy(a);
    entity_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_schema_version();
    test_create_rejects_null_type();
    test_create_accepts_valid();
    test_identity_equals_same_id_and_version();
    test_identity_equals_regardless_of_instance();
    test_identity_not_equal_with_different_id();
    test_same_field_values_different_id_is_not_identity_equal();
    return 0;
}