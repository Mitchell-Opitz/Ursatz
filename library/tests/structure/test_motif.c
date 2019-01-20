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

    EntityID *expected_a = dup_id("event-a");
    EntityID *expected_b = dup_id("event-b");
    assert(entity_id_equals(motif_event_id_at(motif, 0), expected_a));
    assert(entity_id_equals(motif_event_id_at(motif, 1), expected_b));
    entity_id_destroy(expected_a);
    entity_id_destroy(expected_b);

    /* A second Motif instance built from the same id is identity-equal,
       independent of any occurrence -- Motif has no occurrence field to
       even distinguish on. */
    EntityID *other_event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Motif *same_motif;
    assert(motif_create(dup_id("motif-1"), other_event_ids, 2, &same_motif) == MOTIF_OK);
    assert(motif_identity_equals(motif, same_motif));

    /* A different id is not identity-equal, even with identical event refs. */
    EntityID *diff_event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Motif *different_motif;
    assert(motif_create(dup_id("motif-2"), diff_event_ids, 2, &different_motif) == MOTIF_OK);
    assert(!motif_identity_equals(motif, different_motif));

    motif_destroy(motif);
    motif_destroy(same_motif);
    motif_destroy(different_motif);

    printf("test_motif: ok\n");
    return 0;
}