#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "structure/figure.h"

/*
 * Relationship test: a Figure holds its defining events as EntityID
 * references only, never duplicated NoteEvent data, and its identity
 * is stable independent of instance.
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Figure *figure;
    assert(figure_create(dup_id("figure-1"), event_ids, 2, &figure) == FIGURE_OK);

    assert(figure_event_id_count(figure) == 2);

    EntityID *expected_a = dup_id("event-a");
    EntityID *expected_b = dup_id("event-b");
    assert(entity_id_equals(figure_event_id_at(figure, 0), expected_a));
    assert(entity_id_equals(figure_event_id_at(figure, 1), expected_b));
    entity_id_destroy(expected_a);
    entity_id_destroy(expected_b);

    EntityID *other_event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Figure *same_figure;
    assert(figure_create(dup_id("figure-1"), other_event_ids, 2, &same_figure) == FIGURE_OK);
    assert(figure_identity_equals(figure, same_figure));

    EntityID *diff_event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Figure *different_figure;
    assert(figure_create(dup_id("figure-2"), diff_event_ids, 2, &different_figure) == FIGURE_OK);
    assert(!figure_identity_equals(figure, different_figure));

    figure_destroy(figure);
    figure_destroy(same_figure);
    figure_destroy(different_figure);

    printf("test_figure: ok\n");
    return 0;
}