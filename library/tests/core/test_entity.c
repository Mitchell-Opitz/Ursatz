#include "core/entity.h"

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
    Entity *entity = make_entity("work-1", 1, "Work");
    assert(entity != NULL);
    entity_destroy(entity);
}

/* Round-trip: PROVENANCE_REFERENCE_ABSENT survives construction unchanged. */
static void test_provenance_round_trips_absent(void) {
    Entity *entity = make_entity("work-1", 1, "Work");

    const ProvenanceReference *stored = entity_provenance_reference(entity);
    assert(provenance_reference_state(stored) == PROVENANCE_REFERENCE_ABSENT);
    assert(provenance_reference_target_id(stored) == NULL);

    entity_destroy(entity);
}

/* Round-trip: PROVENANCE_REFERENCE_PRESENT and its target EntityID survive
   construction unchanged. */
static void test_provenance_round_trips_present(void) {
    EntityID *id = NULL;
    entity_id_create("work-1", &id);

    SchemaVersion *version = NULL;
    schema_version_create(1, &version);

    EntityID *source_id = NULL;
    entity_id_create("work-0", &source_id);

    ProvenanceReference *provenance = NULL;
    provenance_reference_create_present(source_id, &provenance);

    Entity *entity = NULL;
    EntityResult r = entity_create(id, "Work", version, NULL, provenance, &entity);
    assert(r == ENTITY_OK);

    const ProvenanceReference *stored = entity_provenance_reference(entity);
    assert(provenance_reference_state(stored) == PROVENANCE_REFERENCE_PRESENT);
    assert(strcmp(entity_id_value(provenance_reference_target_id(stored)), "work-0") == 0);

    entity_destroy(entity);
}

/* provenance_reference is lineage, not identity (Law 8/Domain Spec):
   two Entities with the same EntityID+SchemaVersion but different
   provenance are still identity-equal. */
static void test_identity_equals_ignores_differing_provenance(void) {
    EntityID *id_a = NULL;
    EntityID *id_b = NULL;
    entity_id_create("work-1", &id_a);
    entity_id_create("work-1", &id_b);

    SchemaVersion *version_a = NULL;
    SchemaVersion *version_b = NULL;
    schema_version_create(1, &version_a);
    schema_version_create(1, &version_b);

    ProvenanceReference *provenance_a = NULL;
    provenance_reference_create_absent(&provenance_a);

    EntityID *source_id = NULL;
    entity_id_create("work-0", &source_id);
    ProvenanceReference *provenance_b = NULL;
    provenance_reference_create_present(source_id, &provenance_b);

    Entity *a = NULL;
    Entity *b = NULL;
    entity_create(id_a, "Work", version_a, NULL, provenance_a, &a);
    entity_create(id_b, "Work", version_b, NULL, provenance_b, &b);

    assert(entity_identity_equals(a, b));

    entity_destroy(a);
    entity_destroy(b);
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
    test_create_rejects_null_provenance();
    test_create_accepts_valid();
    test_provenance_round_trips_absent();
    test_provenance_round_trips_present();
    test_identity_equals_ignores_differing_provenance();
    test_identity_equals_same_id_and_version();
    test_identity_equals_regardless_of_instance();
    test_identity_not_equal_with_different_id();
    test_same_field_values_different_id_is_not_identity_equal();
    return 0;
}