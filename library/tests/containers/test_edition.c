#include "containers/edition.h"

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
    entity_create(id, "Edition", schema_version, NULL, provenance, &entity);
    return entity;
}

static void test_create_rejects_null_entity(void) {
    EntityID *work_id = NULL;
    entity_id_create("work-1", &work_id);

    Edition *edition = NULL;
    EditionResult r = edition_create(NULL, work_id, &edition);
    assert(r == EDITION_ERR_NULL_ENTITY);
    assert(edition == NULL);

    entity_id_destroy(work_id);
}

static void test_create_rejects_null_work_id(void) {
    Entity *entity = make_entity("edition-1");

    Edition *edition = NULL;
    EditionResult r = edition_create(entity, NULL, &edition);
    assert(r == EDITION_ERR_NULL_WORK_ID);
    assert(edition == NULL);

    entity_destroy(entity);
}

static void test_create_accepts_valid(void) {
    Entity *entity = make_entity("edition-1");
    EntityID *work_id = NULL;
    entity_id_create("work-1", &work_id);

    Edition *edition = NULL;
    EditionResult r = edition_create(entity, work_id, &edition);
    assert(r == EDITION_OK);
    assert(edition != NULL);

    edition_destroy(edition);
}

static void test_accessors_return_constructed_values(void) {
    Entity *entity = make_entity("edition-1");
    EntityID *work_id = NULL;
    entity_id_create("work-1", &work_id);

    Edition *edition = NULL;
    edition_create(entity, work_id, &edition);

    assert(entity_identity_equals(edition_entity(edition), entity));
    assert(entity_id_equals(edition_work_id(edition), work_id));

    edition_destroy(edition);
}

static void test_identity_equals_same_entity_identity(void) {
    Entity *entity_a = make_entity("edition-1");
    Entity *entity_b = make_entity("edition-1");
    EntityID *work_id_a = NULL;
    entity_id_create("work-1", &work_id_a);
    EntityID *work_id_b = NULL;
    entity_id_create("work-2", &work_id_b);

    Edition *a = NULL;
    edition_create(entity_a, work_id_a, &a);
    Edition *b = NULL;
    edition_create(entity_b, work_id_b, &b);

    assert(a != b);
    assert(edition_identity_equals(a, b));

    edition_destroy(a);
    edition_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Entity *entity_a = make_entity("edition-1");
    Entity *entity_b = make_entity("edition-2");
    EntityID *work_id_a = NULL;
    entity_id_create("work-1", &work_id_a);
    EntityID *work_id_b = NULL;
    entity_id_create("work-1", &work_id_b);

    Edition *a = NULL;
    edition_create(entity_a, work_id_a, &a);
    Edition *b = NULL;
    edition_create(entity_b, work_id_b, &b);

    assert(!edition_identity_equals(a, b));

    edition_destroy(a);
    edition_destroy(b);
}

int main(void) {
    test_create_rejects_null_entity();
    test_create_rejects_null_work_id();
    test_create_accepts_valid();
    test_accessors_return_constructed_values();
    test_identity_equals_same_entity_identity();
    test_identity_not_equal_with_different_id();
    return 0;
}