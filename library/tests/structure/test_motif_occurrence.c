#include <assert.h>
#include <stdio.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"
#include "structure/motif_occurrence.h"

/*
 * Relationship test: a MotifOccurrence resolves back to its Motif (by
 * id, via Occurrence's target_id) and to its own concrete event
 * region, and multiple occurrences of the same Motif remain distinct
 * Occurrence instances (distinct own id) while pointing at the same
 * Motif identity.
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static MotifOccurrence *make_occurrence(const char *own_id, const char *motif_id, const char *region_event_id) {
    Occurrence *occurrence;
    assert(occurrence_create(dup_id(own_id), dup_id(motif_id), &occurrence) == OCCURRENCE_OK);

    EntityID *region_event_ids[1] = {dup_id(region_event_id)};
    MotifOccurrence *motif_occurrence;
    assert(motif_occurrence_create(occurrence, region_event_ids, 1, &motif_occurrence) == MOTIF_OCCURRENCE_OK);
    return motif_occurrence;
}

int main(void) {
    MotifOccurrence *first = make_occurrence("occurrence-1", "motif-1", "event-a");
    MotifOccurrence *second = make_occurrence("occurrence-2", "motif-1", "event-b");

    EntityID *expected_motif_id = dup_id("motif-1");
    assert(entity_id_equals(motif_occurrence_motif_id(first), expected_motif_id));
    assert(entity_id_equals(motif_occurrence_motif_id(second), expected_motif_id));
    entity_id_destroy(expected_motif_id);

    /* Both occurrences share the same Motif identity ... */
    assert(entity_id_equals(motif_occurrence_motif_id(first), motif_occurrence_motif_id(second)));
    /* ... but remain distinct Occurrence instances. */
    assert(!entity_id_equals(motif_occurrence_id(first), motif_occurrence_id(second)));

    assert(motif_occurrence_region_event_id_count(first) == 1);
    EntityID *expected_region_a = dup_id("event-a");
    assert(entity_id_equals(motif_occurrence_region_event_id_at(first, 0), expected_region_a));
    entity_id_destroy(expected_region_a);

    EntityID *expected_region_b = dup_id("event-b");
    assert(entity_id_equals(motif_occurrence_region_event_id_at(second, 0), expected_region_b));
    entity_id_destroy(expected_region_b);

    motif_occurrence_destroy(first);
    motif_occurrence_destroy(second);

    printf("test_motif_occurrence: ok\n");
    return 0;
}