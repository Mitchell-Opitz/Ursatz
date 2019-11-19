#include "events/articulation.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    entity_id_create(value, &id);
    return id;
}

static void test_create_rejects_null_kind(void) {
    EntityID *event_id = make_id("note-1");

    Articulation *articulation = NULL;
    ArticulationResult r = articulation_create(NULL, event_id, &articulation);
    assert(r == ARTICULATION_ERR_NULL_KIND);
    assert(articulation == NULL);

    entity_id_destroy(event_id);
}

static void test_create_rejects_empty_kind(void) {
    EntityID *event_id = make_id("note-1");

    Articulation *articulation = NULL;
    ArticulationResult r = articulation_create("", event_id, &articulation);
    assert(r == ARTICULATION_ERR_NULL_KIND);
    assert(articulation == NULL);

    entity_id_destroy(event_id);
}

static void test_create_rejects_null_event_id(void) {
    Articulation *articulation = NULL;
    ArticulationResult r = articulation_create("staccato", NULL, &articulation);
    assert(r == ARTICULATION_ERR_NULL_EVENT_ID);
    assert(articulation == NULL);
}

static void test_create_accepts_valid(void) {
    Articulation *articulation = NULL;
    ArticulationResult r = articulation_create("staccato", make_id("note-1"), &articulation);
    assert(r == ARTICULATION_OK);
    assert(articulation != NULL);
    assert(strcmp(articulation_kind(articulation), "staccato") == 0);

    EntityID *expected = make_id("note-1");
    assert(entity_id_equals(articulation_event_id(articulation), expected));
    entity_id_destroy(expected);

    articulation_destroy(articulation);
}

/* kind is not a closed enum: any non-empty label is accepted, matching
   the Registry's "staccato, tenuto, accent, etc." being non-exhaustive. */
static void test_create_accepts_arbitrary_kind(void) {
    Articulation *articulation = NULL;
    ArticulationResult r = articulation_create("marcato", make_id("note-1"), &articulation);
    assert(r == ARTICULATION_OK);
    assert(strcmp(articulation_kind(articulation), "marcato") == 0);

    articulation_destroy(articulation);
}

int main(void) {
    test_create_rejects_null_kind();
    test_create_rejects_empty_kind();
    test_create_rejects_null_event_id();
    test_create_accepts_valid();
    test_create_accepts_arbitrary_kind();
    printf("test_articulation: ok\n");
    return 0;
}