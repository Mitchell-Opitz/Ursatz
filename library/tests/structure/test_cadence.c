#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "structure/cadence.h"

/*
 * Structural/invariant tests only (construction, event-ref
 * non-duplication, identity) -- Phase-5 shell. No AnalysisRegression
 * (Phase 8) tests here: this file does not assert anything about
 * detecting or classifying a cadence, only about the data shape.
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    /* Construction without a harmonic context (nullable, never blocks
       construction). */
    EntityID *event_ids[2] = {dup_id("event-a"), dup_id("event-b")};
    Cadence *cadence;
    assert(cadence_create(dup_id("cadence-1"), event_ids, 2, NULL, &cadence) == CADENCE_OK);
    assert(cadence_optional_harmonic_context(cadence) == NULL);

    /* Event refs are held without duplication. */
    assert(cadence_event_id_count(cadence) == 2);
    EntityID *expected_a = dup_id("event-a");
    EntityID *expected_b = dup_id("event-b");
    assert(entity_id_equals(cadence_event_id_at(cadence, 0), expected_a));
    assert(entity_id_equals(cadence_event_id_at(cadence, 1), expected_b));
    entity_id_destroy(expected_a);
    entity_id_destroy(expected_b);

    /* Construction with an opaque harmonic-context placeholder. */
    const char *token = "placeholder";
    Value *harmonic_context;
    assert(value_create("HarmonicContext", token, strlen(token) + 1, &harmonic_context) == VALUE_OK);

    EntityID *other_event_ids[1] = {dup_id("event-c")};
    Cadence *cadence_with_context;
    assert(cadence_create(dup_id("cadence-2"), other_event_ids, 1, harmonic_context, &cadence_with_context) ==
           CADENCE_OK);
    assert(cadence_optional_harmonic_context(cadence_with_context) != NULL);

    /* Identity equality: same id, regardless of instance. */
    EntityID *other_event_ids_2[1] = {dup_id("event-d")};
    Cadence *same_id_cadence;
    assert(cadence_create(dup_id("cadence-1"), other_event_ids_2, 1, NULL, &same_id_cadence) == CADENCE_OK);
    assert(cadence_identity_equals(cadence, same_id_cadence));
    assert(!cadence_identity_equals(cadence, cadence_with_context));

    cadence_destroy(cadence);
    cadence_destroy(cadence_with_context);
    cadence_destroy(same_id_cadence);

    printf("test_cadence: ok\n");
    return 0;
}