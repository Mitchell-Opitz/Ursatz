#include "containers/arrangement.h"

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
    entity_create(id, "Arrangement", schema_version, NULL, provenance, &entity);
    return entity;
}

static void test_create_rejects_null_entity(void) {
    EntityID *work_id = NULL;
    entity_id_create("work-1", &work_id);

    Arrangement *arrangement = NULL;
    ArrangementResult r = arrangement_create(NULL, work_id, &arrangement);
    assert(r == ARRANGEMENT_ERR_NULL_ENTITY);
    assert(arrangement == NULL);

    entity_id_destroy(work_id);
}

static void test_create_rejects_null_work_id(void) {
    Entity *entity = make_entity("arrangement-1");

    Arrangement *arrangement = NULL;
    ArrangementResult r = arrangement_create(entity, NULL, &arrangement);
    assert(r == ARRANGEMENT_ERR_NULL_WORK_ID);
    assert(arrangement == NULL);

    entity_destroy(entity);
}

static void test_create_accepts_valid(void) {
    Entity *entity = make_entity("arrangement-1");
    EntityID *work_id = NULL;
    entity_id_create("work-1", &work_id);

    Arrangement *arrangement = NULL;
    ArrangementResult r = arrangement_create(entity, work_id, &arrangement);
    assert(r == ARRANGEMENT_OK);
    assert(arrangement != NULL);

    arrangement_destroy(arrangement);
}

static void test_accessors_return_constructed_values(void) {
    Entity *entity = make_entity("arrangement-1");
    EntityID *work_id = NULL;
    entity_id_create("work-1", &work_id);

    Arrangement *arrangement = NULL;
    arrangement_create(entity, work_id, &arrangement);

    assert(entity_identity_equals(arrangement_entity(arrangement), entity));
    assert(entity_id_equals(arrangement_work_id(arrangement), work_id));

    arrangement_destroy(arrangement);
}

static void test_identity_equals_same_entity_identity(void) {
    Entity *entity_a = make_entity("arrangement-1");
    Entity *entity_b = make_entity("arrangement-1");
    EntityID *work_id_a = NULL;
    entity_id_create("work-1", &work_id_a);
    EntityID *work_id_b = NULL;
    entity_id_create("work-2", &work_id_b);

    Arrangement *a = NULL;
    arrangement_create(entity_a, work_id_a, &a);
    Arrangement *b = NULL;
    arrangement_create(entity_b, work_id_b, &b);

    assert(a != b);
    assert(arrangement_identity_equals(a, b));

    arrangement_destroy(a);
    arrangement_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    Entity *entity_a = make_entity("arrangement-1");
    Entity *entity_b = make_entity("arrangement-2");
    EntityID *work_id_a = NULL;
    entity_id_create("work-1", &work_id_a);
    EntityID *work_id_b = NULL;
    entity_id_create("work-1", &work_id_b);

    Arrangement *a = NULL;
    arrangement_create(entity_a, work_id_a, &a);
    Arrangement *b = NULL;
    arrangement_create(entity_b, work_id_b, &b);

    assert(!arrangement_identity_equals(a, b));

    arrangement_destroy(a);
    arrangement_destroy(b);
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