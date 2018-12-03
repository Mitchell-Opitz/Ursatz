#include "core/occurrence.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_null_id(void) {
    EntityID *target_id = NULL;
    entity_id_create("motif-1", &target_id);

    Occurrence *occurrence = NULL;
    OccurrenceResult r = occurrence_create(NULL, target_id, &occurrence);
    assert(r == OCCURRENCE_ERR_NULL_ID);
    assert(occurrence == NULL);

    entity_id_destroy(target_id);
}

static void test_create_rejects_null_target_id(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1-occurrence-1", &id);

    Occurrence *occurrence = NULL;
    OccurrenceResult r = occurrence_create(id, NULL, &occurrence);
    assert(r == OCCURRENCE_ERR_NULL_TARGET_ID);
    assert(occurrence == NULL);

    entity_id_destroy(id);
}

static void test_create_accepts_valid(void) {
    EntityID *id = NULL;
    entity_id_create("motif-1-occurrence-1", &id);
    EntityID *target_id = NULL;
    entity_id_create("motif-1", &target_id);

    Occurrence *occurrence = NULL;
    OccurrenceResult r = occurrence_create(id, target_id, &occurrence);
    assert(r == OCCURRENCE_OK);
    assert(occurrence != NULL);

    occurrence_destroy(occurrence);
}

/* Context: an Occurrence references its target concept by EntityID only —
   it does not duplicate the target concept, just points at it. */
static void test_references_target_without_duplicating(void) {
    EntityID *target_id = NULL;
    entity_id_create("motif-1", &target_id);

    EntityID *occurrence_id_value = NULL;
    entity_id_create("motif-1-occurrence-1", &occurrence_id_value);
    EntityID *occurrence_target_ref = NULL;
    entity_id_create("motif-1", &occurrence_target_ref);

    Occurrence *occurrence = NULL;
    occurrence_create(occurrence_id_value, occurrence_target_ref, &occurrence);

    assert(entity_id_equals(occurrence_target_id(occurrence), target_id));
    assert(occurrence_target_id(occurrence) != target_id);

    entity_id_destroy(target_id);
    occurrence_destroy(occurrence);
}

/* Multiple situated occurrences of the same concept are distinct
   instances (different occurrence id) that reference the same target. */
static void test_distinct_occurrences_of_same_target(void) {
    EntityID *id_a = NULL;
    entity_id_create("motif-1-occurrence-1", &id_a);
    EntityID *target_a = NULL;
    entity_id_create("motif-1", &target_a);

    EntityID *id_b = NULL;
    entity_id_create("motif-1-occurrence-2", &id_b);
    EntityID *target_b = NULL;
    entity_id_create("motif-1", &target_b);

    Occurrence *occurrence_a = NULL;
    occurrence_create(id_a, target_a, &occurrence_a);
    Occurrence *occurrence_b = NULL;
    occurrence_create(id_b, target_b, &occurrence_b);

    assert(!entity_id_equals(occurrence_id(occurrence_a), occurrence_id(occurrence_b)));
    assert(entity_id_equals(occurrence_target_id(occurrence_a), occurrence_target_id(occurrence_b)));

    occurrence_destroy(occurrence_a);
    occurrence_destroy(occurrence_b);
}

int main(void) {
    test_create_rejects_null_id();
    test_create_rejects_null_target_id();
    test_create_accepts_valid();
    test_references_target_without_duplicating();
    test_distinct_occurrences_of_same_target();
    return 0;
}