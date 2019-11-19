#include <assert.h>
#include <stdio.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"
#include "structure/phrase_occurrence.h"

/*
 * Relationship test: a PhraseOccurrence resolves back to its Phrase
 * (by id, via Occurrence's target_id) and to its own concrete event
 * region, and multiple occurrences of the same Phrase remain distinct
 * Occurrence instances (distinct own id) while pointing at the same
 * Phrase identity.
 */

static EntityID *dup_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static PhraseOccurrence *make_occurrence(const char *own_id, const char *phrase_id, const char *region_event_id) {
    Occurrence *occurrence;
    assert(occurrence_create(dup_id(own_id), dup_id(phrase_id), &occurrence) == OCCURRENCE_OK);

    EntityID *region_event_ids[1] = {dup_id(region_event_id)};
    PhraseOccurrence *phrase_occurrence;
    assert(phrase_occurrence_create(occurrence, region_event_ids, 1, &phrase_occurrence) == PHRASE_OCCURRENCE_OK);
    return phrase_occurrence;
}

int main(void) {
    PhraseOccurrence *first = make_occurrence("occurrence-1", "phrase-1", "event-a");
    PhraseOccurrence *second = make_occurrence("occurrence-2", "phrase-1", "event-b");

    EntityID *expected_phrase_id = dup_id("phrase-1");
    assert(entity_id_equals(phrase_occurrence_phrase_id(first), expected_phrase_id));
    assert(entity_id_equals(phrase_occurrence_phrase_id(second), expected_phrase_id));
    entity_id_destroy(expected_phrase_id);

    /* Both occurrences share the same Phrase identity ... */
    assert(entity_id_equals(phrase_occurrence_phrase_id(first), phrase_occurrence_phrase_id(second)));
    /* ... but remain distinct Occurrence instances. */
    assert(!entity_id_equals(phrase_occurrence_id(first), phrase_occurrence_id(second)));

    assert(phrase_occurrence_region_event_id_count(first) == 1);
    EntityID *expected_region_a = dup_id("event-a");
    assert(entity_id_equals(phrase_occurrence_region_event_id_at(first, 0), expected_region_a));
    entity_id_destroy(expected_region_a);

    EntityID *expected_region_b = dup_id("event-b");
    assert(entity_id_equals(phrase_occurrence_region_event_id_at(second, 0), expected_region_b));
    entity_id_destroy(expected_region_b);

    phrase_occurrence_destroy(first);
    phrase_occurrence_destroy(second);

    printf("test_phrase_occurrence: ok\n");
    return 0;
}