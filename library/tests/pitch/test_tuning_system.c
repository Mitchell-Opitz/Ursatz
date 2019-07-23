#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "pitch/tuning_system.h"

static void test_create_rejects_null_id(void) {
    TuningSystem *tuning_system = NULL;
    TuningSystemResult r = tuning_system_create(NULL, "12-TET", &tuning_system);
    assert(r == TUNING_SYSTEM_ERR_NULL_ID);
    assert(tuning_system == NULL);
}

static void test_create_rejects_null_name(void) {
    EntityID *id = NULL;
    entity_id_create("tuning-12tet", &id);

    TuningSystem *tuning_system = NULL;
    TuningSystemResult r = tuning_system_create(id, NULL, &tuning_system);
    assert(r == TUNING_SYSTEM_ERR_NULL_NAME);
    assert(tuning_system == NULL);

    entity_id_destroy(id);
}

static void test_create_rejects_empty_name(void) {
    EntityID *id = NULL;
    entity_id_create("tuning-12tet", &id);

    TuningSystem *tuning_system = NULL;
    TuningSystemResult r = tuning_system_create(id, "", &tuning_system);
    assert(r == TUNING_SYSTEM_ERR_NULL_NAME);
    assert(tuning_system == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_valid(void) {
    EntityID *id = NULL;
    entity_id_create("tuning-12tet", &id);

    TuningSystem *tuning_system = NULL;
    TuningSystemResult r = tuning_system_create(id, "12-TET", &tuning_system);
    assert(r == TUNING_SYSTEM_OK);
    assert(tuning_system != NULL);
    assert(strcmp(tuning_system_name(tuning_system), "12-TET") == 0);

    tuning_system_destroy(tuning_system);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    EntityID *id_b = NULL;
    entity_id_create("tuning-12tet", &id_a);
    entity_id_create("tuning-12tet", &id_b);

    TuningSystem *a = NULL;
    TuningSystem *b = NULL;
    tuning_system_create(id_a, "12-TET", &a);
    tuning_system_create(id_b, "12-TET (duplicate)", &b);

    assert(a != b);
    assert(tuning_system_identity_equals(a, b));

    tuning_system_destroy(a);
    tuning_system_destroy(b);
}

static void test_identity_not_equal_different_id(void) {
    EntityID *id_a = NULL;
    EntityID *id_b = NULL;
    entity_id_create("tuning-12tet", &id_a);
    entity_id_create("tuning-just-intonation", &id_b);

    TuningSystem *a = NULL;
    TuningSystem *b = NULL;
    tuning_system_create(id_a, "12-TET", &a);
    tuning_system_create(id_b, "just intonation", &b);

    assert(!tuning_system_identity_equals(a, b));

    tuning_system_destroy(a);
    tuning_system_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_name();
    test_create_rejects_empty_name();
    test_create_accepts_valid();
    test_identity_equals_same_id();
    test_identity_not_equal_different_id();
    printf("test_tuning_system: ok\n");
    return 0;
}