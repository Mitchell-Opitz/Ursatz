#include "containers/phrase.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Phrase *phrase = NULL;
    PhraseResult r = phrase_create(NULL, NULL, 0, &phrase);
    assert(r == PHRASE_ERR_NULL_ID);
    assert(phrase == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("phrase-1", &id);

    Phrase *phrase = NULL;
    PhraseResult r = phrase_create(id, NULL, 0, &phrase);
    assert(r == PHRASE_OK);
    assert(phrase != NULL);
    assert(phrase_event_id_count(phrase) == 0);

    phrase_destroy(phrase);
}

static void test_create_rejects_null_event_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("phrase-1", &id);

    EntityID *event_ids[1] = {NULL};

    Phrase *phrase = NULL;
    PhraseResult r = phrase_create(id, event_ids, 1, &phrase);
    assert(r == PHRASE_ERR_NULL_EVENT_ID);
    assert(phrase == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_event_references(void) {
    EntityID *id = NULL;
    entity_id_create("phrase-1", &id);

    EntityID *event_id_1 = NULL;
    entity_id_create("event-1", &event_id_1);
    EntityID *event_id_2 = NULL;
    entity_id_create("event-2", &event_id_2);
    EntityID *event_ids[2] = {event_id_1, event_id_2};

    Phrase *phrase = NULL;
    PhraseResult r = phrase_create(id, event_ids, 2, &phrase);
    assert(r == PHRASE_OK);
    assert(phrase != NULL);

    assert(phrase_event_id_count(phrase) == 2);
    assert(entity_id_equals(phrase_event_id_at(phrase, 0), event_id_1));
    assert(entity_id_equals(phrase_event_id_at(phrase, 1), event_id_2));

    phrase_destroy(phrase);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("phrase-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("phrase-1", &id_b);

    Phrase *a = NULL;
    phrase_create(id_a, NULL, 0, &a);
    Phrase *b = NULL;
    phrase_create(id_b, NULL, 0, &b);

    assert(a != b);
    assert(phrase_identity_equals(a, b));

    phrase_destroy(a);
    phrase_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("phrase-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("phrase-2", &id_b);

    Phrase *a = NULL;
    phrase_create(id_a, NULL, 0, &a);
    Phrase *b = NULL;
    phrase_create(id_b, NULL, 0, &b);

    assert(!phrase_identity_equals(a, b));

    phrase_destroy(a);
    phrase_destroy(b);
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