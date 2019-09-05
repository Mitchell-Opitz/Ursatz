#include "containers/work.h"

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
    entity_create(id, "Work", schema_version, NULL, provenance, &entity);
    return entity;
}

static void test_create_rejects_null_entity(void) {
    Work *work = NULL;
    WorkResult r = work_create(NULL, NULL, 0, &work);
    assert(r == WORK_ERR_NULL_ENTITY);
    assert(work == NULL);
}

static void test_create_accepts_empty_movement_ids(void) {
    Entity *entity = make_entity("work-1");

    Work *work = NULL;
    WorkResult r = work_create(entity, NULL, 0, &work);
    assert(r == WORK_OK);
    assert(work != NULL);
    assert(work_movement_id_count(work) == 0);

    work_destroy(work);
}

static void test_create_rejects_null_movement_id_entry(void) {
    Entity *entity = make_entity("work-1");

    EntityID *movement_ids[1] = {NULL};

    Work *work = NULL;
    WorkResult r = work_create(entity, movement_ids, 1, &work);
    assert(r == WORK_ERR_NULL_MOVEMENT_ID);
    assert(work == NULL);

    entity_destroy(entity);
}

static void test_create_accepts_movement_references(void) {
    Entity *entity = make_entity("work-1");

    EntityID *movement_id_1 = NULL;
    entity_id_create("movement-1", &movement_id_1);
    EntityID *movement_id_2 = NULL;
    entity_id_create("movement-2", &movement_id_2);
    EntityID *movement_ids[2] = {movement_id_1, movement_id_2};

    Work *work = NULL;
    WorkResult r = work_create(entity, movement_ids, 2, &work);
    assert(r == WORK_OK);
    assert(work != NULL);

    assert(work_movement_id_count(work) == 2);
    assert(entity_id_equals(work_movement_id_at(work, 0), movement_id_1));
    assert(entity_id_equals(work_movement_id_at(work, 1), movement_id_2));

    work_destroy(work);
}

static void test_entity_accessor_returns_constructed_entity(void) {
    Entity *entity = make_entity("work-1");
    Work *work = NULL;
    work_create(entity, NULL, 0, &work);

    const Entity *returned = work_entity(work);
    assert(returned != NULL);
    assert(entity_identity_equals(returned, entity));

    work_destroy(work);
}

static void test_identity_equals_same_entity_identity(void) {
    Entity *entity_a = make_entity("work-1");
    Entity *entity_b = make_entity("work-1");

    Work *a = NULL;
    work_create(entity_a, NULL, 0, &a);
    Work *b = NULL;
    work_create(entity_b, NULL, 0, &b);

    assert(a != b);
    assert(work_identity_equals(a, b));

    work_destroy(a);
    work_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Entity *entity_a = make_entity("work-1");
    Entity *entity_b = make_entity("work-2");

    Work *a = NULL;
    work_create(entity_a, NULL, 0, &a);
    Work *b = NULL;
    work_create(entity_b, NULL, 0, &b);

    assert(!work_identity_equals(a, b));

    work_destroy(a);
    work_destroy(b);
}

int main(void) {
    test_create_rejects_null_entity();
    test_create_accepts_empty_movement_ids();
    test_create_rejects_null_movement_id_entry();
    test_create_accepts_movement_references();
    test_entity_accessor_returns_constructed_entity();
    test_identity_equals_same_entity_identity();
    test_identity_not_equal_with_different_id();
    return 0;
}