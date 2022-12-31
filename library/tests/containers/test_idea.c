#include "containers/idea.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    Idea *idea = NULL;
    IdeaResult r = idea_create(NULL, NULL, 0, &idea);
    assert(r == IDEA_ERR_NULL_ID);
    assert(idea == NULL);
}

static void test_create_accepts_empty_references(void) {
    EntityID *id = NULL;
    entity_id_create("idea-1", &id);

    Idea *idea = NULL;
    IdeaResult r = idea_create(id, NULL, 0, &idea);
    assert(r == IDEA_OK);
    assert(idea != NULL);
    assert(idea_event_id_count(idea) == 0);

    idea_destroy(idea);
}

static void test_create_rejects_null_event_id_entry(void) {
    EntityID *id = NULL;
    entity_id_create("idea-1", &id);

    EntityID *event_ids[1] = {NULL};

    Idea *idea = NULL;
    IdeaResult r = idea_create(id, event_ids, 1, &idea);
    assert(r == IDEA_ERR_NULL_EVENT_ID);
    assert(idea == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_event_references(void) {
    EntityID *id = NULL;
    entity_id_create("idea-1", &id);

    EntityID *event_id_1 = NULL;
    entity_id_create("event-1", &event_id_1);
    EntityID *event_id_2 = NULL;
    entity_id_create("event-2", &event_id_2);
    EntityID *event_ids[2] = {event_id_1, event_id_2};

    Idea *idea = NULL;
    IdeaResult r = idea_create(id, event_ids, 2, &idea);
    assert(r == IDEA_OK);
    assert(idea != NULL);

    assert(idea_event_id_count(idea) == 2);
    assert(entity_id_equals(idea_event_id_at(idea, 0), event_id_1));
    assert(entity_id_equals(idea_event_id_at(idea, 1), event_id_2));

    idea_destroy(idea);
}

static void test_identity_equals_same_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("idea-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("idea-1", &id_b);

    Idea *a = NULL;
    idea_create(id_a, NULL, 0, &a);
    Idea *b = NULL;
    idea_create(id_b, NULL, 0, &b);

    assert(a != b);
    assert(idea_identity_equals(a, b));

    idea_destroy(a);
    idea_destroy(b);
}

static void test_identity_not_equal_with_different_id(void) {
    EntityID *id_a = NULL;
    entity_id_create("idea-1", &id_a);
    EntityID *id_b = NULL;
    entity_id_create("idea-2", &id_b);

    Idea *a = NULL;
    idea_create(id_a, NULL, 0, &a);
    Idea *b = NULL;
    idea_create(id_b, NULL, 0, &b);

    assert(!idea_identity_equals(a, b));

    idea_destroy(a);
    idea_destroy(b);
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