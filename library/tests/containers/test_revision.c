#include "containers/revision.h"

#include <assert.h>
#include <stddef.h>

static Entity *make_entity(const char *id_value) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);
    SchemaVersion *schema_version = NULL;
    schema_version_create(1, &schema_version);
    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    Entity *entity = NULL;
    entity_create(id, "Revision", schema_version, NULL, provenance, &entity);
    return entity;
}

static Version *make_version(const char *target_value, unsigned long revision_number) {
    EntityID *target = NULL;
    entity_id_create(target_value, &target);

    Version *version = NULL;
    version_create(target, revision_number, &version);
    entity_id_destroy(target);
    return version;
}

static void test_create_rejects_null_entity(void) {
    EntityID *target_id = NULL;
    entity_id_create("work-1", &target_id);
    Version *version = make_version("work-1", 1);

    Revision *revision = NULL;
    RevisionResult r = revision_create(NULL, target_id, version, &revision);
    assert(r == REVISION_ERR_NULL_ENTITY);
    assert(revision == NULL);

    entity_id_destroy(target_id);
    version_destroy(version);
}

static void test_create_rejects_null_target_id(void) {
    Entity *entity = make_entity("revision-1");
    Version *version = make_version("work-1", 1);

    Revision *revision = NULL;
    RevisionResult r = revision_create(entity, NULL, version, &revision);
    assert(r == REVISION_ERR_NULL_TARGET_ID);
    assert(revision == NULL);

    entity_destroy(entity);
    version_destroy(version);
}

static void test_create_rejects_null_version(void) {
    Entity *entity = make_entity("revision-1");
    EntityID *target_id = NULL;
    entity_id_create("work-1", &target_id);

    Revision *revision = NULL;
    RevisionResult r = revision_create(entity, target_id, NULL, &revision);
    assert(r == REVISION_ERR_NULL_VERSION);
    assert(revision == NULL);

    entity_destroy(entity);
    entity_id_destroy(target_id);
}

static void test_create_accepts_valid(void) {
    Entity *entity = make_entity("revision-1");
    EntityID *target_id = NULL;
    entity_id_create("work-1", &target_id);
    Version *version = make_version("work-1", 2);

    Revision *revision = NULL;
    RevisionResult r = revision_create(entity, target_id, version, &revision);
    assert(r == REVISION_OK);
    assert(revision != NULL);

    revision_destroy(revision);
}

static void test_accessors_return_constructed_values(void) {
    Entity *entity = make_entity("revision-1");
    EntityID *target_id = NULL;
    entity_id_create("work-1", &target_id);
    Version *version = make_version("work-1", 2);

    Revision *revision = NULL;
    revision_create(entity, target_id, version, &revision);

    assert(entity_identity_equals(revision_entity(revision), entity));
    assert(entity_id_equals(revision_target_id(revision), target_id));
    assert(version_revision(revision_version(revision)) == 2);

    revision_destroy(revision);
}

static void test_identity_equals_same_entity_identity(void) {
    Entity *entity_a = make_entity("revision-1");
    Entity *entity_b = make_entity("revision-1");
    EntityID *target_id_a = NULL;
    entity_id_create("work-1", &target_id_a);
    EntityID *target_id_b = NULL;
    entity_id_create("work-2", &target_id_b);
    Version *version_a = make_version("work-1", 1);
    Version *version_b = make_version("work-2", 2);

    Revision *a = NULL;
    revision_create(entity_a, target_id_a, version_a, &a);
    Revision *b = NULL;
    revision_create(entity_b, target_id_b, version_b, &b);

    assert(a != b);
    assert(revision_identity_equals(a, b));

    revision_destroy(a);
    revision_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Entity *entity_a = make_entity("revision-1");
    Entity *entity_b = make_entity("revision-2");
    EntityID *target_id_a = NULL;
    entity_id_create("work-1", &target_id_a);
    EntityID *target_id_b = NULL;
    entity_id_create("work-1", &target_id_b);
    Version *version_a = make_version("work-1", 1);
    Version *version_b = make_version("work-1", 1);

    Revision *a = NULL;
    revision_create(entity_a, target_id_a, version_a, &a);
    Revision *b = NULL;
    revision_create(entity_b, target_id_b, version_b, &b);

    assert(!revision_identity_equals(a, b));

    revision_destroy(a);
    revision_destroy(b);
}

int main(void) {
    test_create_rejects_null_entity();
    test_create_rejects_null_target_id();
    test_create_rejects_null_version();
    test_create_accepts_valid();
    test_accessors_return_constructed_values();
    test_identity_equals_same_entity_identity();
    test_identity_not_equal_with_different_id();
    return 0;
}