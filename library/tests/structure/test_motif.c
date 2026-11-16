#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "structure/motif.h"

/*
 * Relationship test: a Motif holds its defining events as EntityID
 * references only, never duplicated NoteEvent data, and its identity
 * is stable independent of any occurrence of it (Motif carries no
 * occurrence-related field at all, so identity equality here reduces
 * to id equality regardless of how many times the same ids are reused
 * to build a separate Motif instance).
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Motif *motif;
    assert(motif_create(dup_id("motif-1"), event_ids, 2, &motif) == MOTIF_OK);

    assert(motif_event_id_count(motif) == 2);

    EntityID *expected_id = dup_id("motif-1");
    assert(entity_id_equals(motif_id(motif), expected_id));
    entity_id_destroy(expected_id);

    /* A second Motif built from the same event refs under another id keeps its own id. */
    EntityID *other_event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Motif *other_motif;
    assert(motif_create(dup_id("motif-2"), other_event_ids, 2, &other_motif) == MOTIF_OK);
    assert(motif_event_id_count(other_motif) == 2);
    assert(!entity_id_equals(motif_id(motif), motif_id(other_motif)));

    motif_destroy(motif);
    motif_destroy(other_motif);

    printf("test_motif: ok\n");
    return 0;
}