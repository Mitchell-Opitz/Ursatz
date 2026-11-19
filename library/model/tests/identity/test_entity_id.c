#include "model/identity/entity_id.h"

#include <assert.h>
#include <string.h>

static void test_create_rejects_null(void) {
    EntityID *id = NULL;
    EntityIDResult r = entity_id_create(NULL, &id);
    assert(r == ENTITY_ID_ERR_EMPTY);
    assert(id == NULL);
}

static void test_create_rejects_empty(void) {
    EntityID *id = NULL;
    EntityIDResult r = entity_id_create("", &id);
    assert(r == ENTITY_ID_ERR_EMPTY);
    assert(id == NULL);
}

static void test_create_accepts_valid(void) {
    EntityID *id = NULL;
    EntityIDResult r = entity_id_create("motif-3f9a2b", &id);
    assert(r == ENTITY_ID_OK);
    assert(id != NULL);
    entity_id_destroy(id);
}

static void test_equals_same_value(void) {
    EntityID *a = NULL;
    EntityID *b = NULL;
    entity_id_create("note-8e92c1", &a);
    entity_id_create("note-8e92c1", &b);
    assert(entity_id_equals(a, b));
    entity_id_destroy(a);
    entity_id_destroy(b);
}

static void test_equals_different_value(void) {
    EntityID *a = NULL;
    EntityID *b = NULL;
    entity_id_create("note-8e92c1", &a);
    entity_id_create("note-0000000", &b);
    assert(!entity_id_equals(a, b));
    entity_id_destroy(a);
    entity_id_destroy(b);
}

static void test_round_trip(void) {
    EntityID *original = NULL;
    entity_id_create("motif-3f9a2b", &original);

    const char *serialized = entity_id_value(original);

    EntityID *restored = NULL;
    entity_id_create(serialized, &restored);

    assert(entity_id_equals(original, restored));

    entity_id_destroy(original);
    entity_id_destroy(restored);
}

static void test_copy(void) {
    EntityID *id = NULL;
    assert(entity_id_create("note-7", &id) == ENTITY_ID_OK);
    EntityID *copy = entity_id_copy(id);
    assert(copy != NULL && copy != id);
    assert(entity_id_equals(id, copy));
    assert(strcmp(entity_id_value(copy), "note-7") == 0);
    assert(entity_id_copy(NULL) == NULL);
    entity_id_destroy(copy);
    entity_id_destroy(id);
}

int main(void) {
    test_create_rejects_null();
    test_create_rejects_empty();
    test_create_accepts_valid();
    test_equals_same_value();
    test_equals_different_value();
    test_round_trip();
    test_copy();
    return 0;
}