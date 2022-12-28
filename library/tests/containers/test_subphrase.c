#include "containers/subphrase.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Subphrase *subphrase = NULL;
    SubphraseResult r = subphrase_create(NULL, NULL, 0, &subphrase);
    assert(r == SUBPHRASE_ERR_NULL_ID);
    assert(subphrase == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("subphrase-1", &id);

    Subphrase *subphrase = NULL;
    SubphraseResult r = subphrase_create(id, NULL, 0, &subphrase);
    assert(r == SUBPHRASE_OK);
    assert(subphrase != NULL);
    assert(subphrase_event_id_count(subphrase) == 0);

    subphrase_destroy(subphrase);
}

static void test_create_rejects_null_event_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("subphrase-1", &id);

    EntityID *event_ids[1] = {NULL};

    Subphrase *subphrase = NULL;
    SubphraseResult r = subphrase_create(id, event_ids, 1, &subphrase);
    assert(r == SUBPHRASE_ERR_NULL_EVENT_ID);
    assert(subphrase == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_event_references(void) {
    EntityID *id = NULL;
    entity_id_create("subphrase-1", &id);

    EntityID *event_id_1 = NULL;
    entity_id_create("event-1", &event_id_1);
    EntityID *event_id_2 = NULL;
    entity_id_create("event-2", &event_id_2);
    EntityID *event_ids[2] = {event_id_1, event_id_2};

    Subphrase *subphrase = NULL;
    SubphraseResult r = subphrase_create(id, event_ids, 2, &subphrase);
    assert(r == SUBPHRASE_OK);
    assert(subphrase != NULL);

    assert(subphrase_event_id_count(subphrase) == 2);
    assert(entity_id_equals(subphrase_event_id_at(subphrase, 0), event_id_1));
    assert(entity_id_equals(subphrase_event_id_at(subphrase, 1), event_id_2));

    subphrase_destroy(subphrase);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("subphrase-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("subphrase-1", &id_b);

    Subphrase *a = NULL;
    subphrase_create(id_a, NULL, 0, &a);
    Subphrase *b = NULL;
    subphrase_create(id_b, NULL, 0, &b);

    assert(a != b);
    assert(subphrase_identity_equals(a, b));

    subphrase_destroy(a);
    subphrase_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("subphrase-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("subphrase-2", &id_b);

    Subphrase *a = NULL;
    subphrase_create(id_a, NULL, 0, &a);
    Subphrase *b = NULL;
    subphrase_create(id_b, NULL, 0, &b);

    assert(!subphrase_identity_equals(a, b));

    subphrase_destroy(a);
    subphrase_destroy(b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_accepts_empty_references();
    test_create_rejects_null_event_id_entry();
    test_create_accepts_event_references();
    test_identity_equals_same_id();
    test_identity_not_equal_with_different_id();
    return 0;
}