#include "containers/part.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Part *part = NULL;
    PartResult r = part_create(NULL, NULL, 0, &part);
    assert(r == PART_ERR_NULL_ID);
    assert(part == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("part-1", &id);

    Part *part = NULL;
    PartResult r = part_create(id, NULL, 0, &part);
    assert(r == PART_OK);
    assert(part != NULL);
    assert(part_staff_id_count(part) == 0);

    part_destroy(part);
}

static void test_create_rejects_null_staff_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("part-1", &id);

    EntityID *staff_ids[1] = {NULL};

    Part *part = NULL;
    PartResult r = part_create(id, staff_ids, 1, &part);
    assert(r == PART_ERR_NULL_STAFF_ID);
    assert(part == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_staff_references(void) {
    EntityID *id = NULL;
    entity_id_create("part-1", &id);

    EntityID *staff_id_1 = NULL;
    entity_id_create("staff-1", &staff_id_1);
    EntityID *staff_id_2 = NULL;
    entity_id_create("staff-2", &staff_id_2);
    EntityID *staff_ids[2] = {staff_id_1, staff_id_2};

    Part *part = NULL;
    PartResult r = part_create(id, staff_ids, 2, &part);
    assert(r == PART_OK);
    assert(part != NULL);

    assert(part_staff_id_count(part) == 2);
    assert(entity_id_equals(part_staff_id_at(part, 0), staff_id_1));
    assert(entity_id_equals(part_staff_id_at(part, 1), staff_id_2));

    part_destroy(part);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("part-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("part-1", &id_b);

    Part *a = NULL;
    part_create(id_a, NULL, 0, &a);
    Part *b = NULL;
    part_create(id_b, NULL, 0, &b);

    assert(a != b);
    assert(part_identity_equals(a, b));

    part_destroy(a);
    part_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("part-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("part-2", &id_b);

    Part *a = NULL;
    part_create(id_a, NULL, 0, &a);
    Part *b = NULL;
    part_create(id_b, NULL, 0, &b);

    assert(!part_identity_equals(a, b));

    part_destroy(a);
    part_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_accepts_empty_references();
    test_create_rejects_null_staff_id_entry();
    test_create_accepts_staff_references();
    test_identity_equals_same_id();
    test_identity_not_equal_with_different_id();
    return 0;
}