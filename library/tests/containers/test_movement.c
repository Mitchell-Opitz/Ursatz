#include "containers/movement.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Movement *movement = NULL;
    MovementResult r = movement_create(NULL, NULL, 0, &movement);
    assert(r == MOVEMENT_ERR_NULL_ID);
    assert(movement == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("movement-1", &id);

    Movement *movement = NULL;
    MovementResult r = movement_create(id, NULL, 0, &movement);
    assert(r == MOVEMENT_OK);
    assert(movement != NULL);
    assert(movement_section_id_count(movement) == 0);

    movement_destroy(movement);
}

static void test_create_rejects_null_section_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("movement-1", &id);

    EntityID *section_ids[1] = {NULL};

    Movement *movement = NULL;
    MovementResult r = movement_create(id, section_ids, 1, &movement);
    assert(r == MOVEMENT_ERR_NULL_SECTION_ID);
    assert(movement == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_section_references(void) {
    EntityID *id = NULL;
    entity_id_create("movement-1", &id);

    EntityID *section_id_1 = NULL;
    entity_id_create("section-1", &section_id_1);
    EntityID *section_id_2 = NULL;
    entity_id_create("section-2", &section_id_2);
    EntityID *section_ids[2] = {section_id_1, section_id_2};

    Movement *movement = NULL;
    MovementResult r = movement_create(id, section_ids, 2, &movement);
    assert(r == MOVEMENT_OK);
    assert(movement != NULL);

    assert(movement_section_id_count(movement) == 2);
    assert(entity_id_equals(movement_section_id_at(movement, 0), section_id_1));
    assert(entity_id_equals(movement_section_id_at(movement, 1), section_id_2));

    movement_destroy(movement);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("movement-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("movement-1", &id_b);

    Movement *a = NULL;
    movement_create(id_a, NULL, 0, &a);
    Movement *b = NULL;
    movement_create(id_b, NULL, 0, &b);

    assert(a != b);
    assert(movement_identity_equals(a, b));

    movement_destroy(a);
    movement_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("movement-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("movement-2", &id_b);

    Movement *a = NULL;
    movement_create(id_a, NULL, 0, &a);
    Movement *b = NULL;
    movement_create(id_b, NULL, 0, &b);

    assert(!movement_identity_equals(a, b));

    movement_destroy(a);
    movement_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_accepts_empty_references();
    test_create_rejects_null_section_id_entry();
    test_create_accepts_section_references();
    test_identity_equals_same_id();
    test_identity_not_equal_with_different_id();
    return 0;
}