#include "containers/staff.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Staff *staff = NULL;
    StaffResult r = staff_create(NULL, NULL, 0, NULL, 0, &staff);
    assert(r == STAFF_ERR_NULL_ID);
    assert(staff == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("staff-1", &id);

    Staff *staff = NULL;
    StaffResult r = staff_create(id, NULL, 0, NULL, 0, &staff);
    assert(r == STAFF_OK);
    assert(staff != NULL);
    assert(staff_voice_id_count(staff) == 0);
    assert(staff_layer_id_count(staff) == 0);

    staff_destroy(staff);
}

static void test_create_rejects_null_voice_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("staff-1", &id);

    EntityID *voice_ids[1] = {NULL};

    Staff *staff = NULL;
    StaffResult r = staff_create(id, voice_ids, 1, NULL, 0, &staff);
    assert(r == STAFF_ERR_NULL_VOICE_ID);
    assert(staff == NULL);

    entity_id_destroy(id);
}

static void test_create_rejects_null_layer_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("staff-1", &id);

    EntityID *layer_ids[1] = {NULL};

    Staff *staff = NULL;
    StaffResult r = staff_create(id, NULL, 0, layer_ids, 1, &staff);
    assert(r == STAFF_ERR_NULL_LAYER_ID);
    assert(staff == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_voice_and_layer_references(void) {
    EntityID *id = NULL;
    entity_id_create("staff-1", &id);

    EntityID *voice_id_1 = NULL;
    entity_id_create("voice-1", &voice_id_1);
    EntityID *voice_id_2 = NULL;
    entity_id_create("voice-2", &voice_id_2);
    EntityID *voice_ids[2] = {voice_id_1, voice_id_2};

    EntityID *layer_id_1 = NULL;
    entity_id_create("layer-1", &layer_id_1);
    EntityID *layer_ids[1] = {layer_id_1};

    Staff *staff = NULL;
    StaffResult r = staff_create(id, voice_ids, 2, layer_ids, 1, &staff);
    assert(r == STAFF_OK);
    assert(staff != NULL);

    assert(staff_voice_id_count(staff) == 2);
    assert(entity_id_equals(staff_voice_id_at(staff, 0), voice_id_1));
    assert(entity_id_equals(staff_voice_id_at(staff, 1), voice_id_2));

    assert(staff_layer_id_count(staff) == 1);
    assert(entity_id_equals(staff_layer_id_at(staff, 0), layer_id_1));

    staff_destroy(staff);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("staff-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("staff-1", &id_b);

    Staff *a = NULL;
    staff_create(id_a, NULL, 0, NULL, 0, &a);
    Staff *b = NULL;
    staff_create(id_b, NULL, 0, NULL, 0, &b);

    assert(a != b);
    assert(staff_identity_equals(a, b));

    staff_destroy(a);
    staff_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("staff-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("staff-2", &id_b);

    Staff *a = NULL;
    staff_create(id_a, NULL, 0, NULL, 0, &a);
    Staff *b = NULL;
    staff_create(id_b, NULL, 0, NULL, 0, &b);

    assert(!staff_identity_equals(a, b));

    staff_destroy(a);
    staff_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_accepts_empty_references();
    test_create_rejects_null_voice_id_entry();
    test_create_rejects_null_layer_id_entry();
    test_create_accepts_voice_and_layer_references();
    test_identity_equals_same_id();
    test_identity_not_equal_with_different_id();
    return 0;
}