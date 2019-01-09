#include "events/event.h"

#include <assert.h>
#include <stddef.h>

static Occurrence *make_occurrence(const char *id_value, const char *target_id_value) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);
    EntityID *target_id = NULL;
    entity_id_create(target_id_value, &target_id);

    Occurrence *occurrence = NULL;
    occurrence_create(id, target_id, &occurrence);
    return occurrence;
}

static TemporalAnchor *make_temporal_anchor(long numerator, unsigned long denominator) {
    MusicalTime *position = NULL;
    musical_time_create(numerator, denominator, &position);

    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);
    return anchor;
}

static void test_create_rejects_null_occurrence(void) {
    TemporalAnchor *anchor = make_temporal_anchor(1, 4);

    Event *event = NULL;
    EventResult r = event_create(NULL, anchor, &event);
    assert(r == EVENT_ERR_NULL_OCCURRENCE);
    assert(event == NULL);

    temporal_anchor_destroy(anchor);
}

static void test_create_rejects_null_temporal_anchor(void) {
    Occurrence *occurrence = make_occurrence("note-1-occurrence-1", "note-1");

    Event *event = NULL;
    EventResult r = event_create(occurrence, NULL, &event);
    assert(r == EVENT_ERR_NULL_TEMPORAL_ANCHOR);
    assert(event == NULL);

    occurrence_destroy(occurrence);
}

static void test_create_accepts_valid(void) {
    Occurrence *occurrence = make_occurrence("note-1-occurrence-1", "note-1");
    TemporalAnchor *anchor = make_temporal_anchor(1, 4);

    Event *event = NULL;
    EventResult r = event_create(occurrence, anchor, &event);
    assert(r == EVENT_OK);
    assert(event != NULL);

    event_destroy(event);
}

/* Structural: an Event composes an Occurrence and a TemporalAnchor without
   collapsing into either — both remain independently accessible with
   their own fields intact. */
static void test_composes_occurrence_and_temporal_anchor(void) {
    Occurrence *occurrence = make_occurrence("note-1-occurrence-1", "note-1");
    TemporalAnchor *anchor = make_temporal_anchor(3, 4);

    Event *event = NULL;
    event_create(occurrence, anchor, &event);

    const Occurrence *event_occ = event_occurrence(event);
    EntityID *expected_target = NULL;
    entity_id_create("note-1", &expected_target);
    assert(entity_id_equals(occurrence_target_id(event_occ), expected_target));
    entity_id_destroy(expected_target);

    const TemporalAnchor *event_anchor = event_temporal_anchor(event);
    const MusicalTime *position = temporal_anchor_position(event_anchor);
    assert(musical_time_numerator(position) == 3);
    assert(musical_time_denominator(position) == 4);

    event_destroy(event);
}

int main(void) {
    test_create_rejects_null_occurrence();
    test_create_rejects_null_temporal_anchor();
    test_create_accepts_valid();
    test_composes_occurrence_and_temporal_anchor();
    return 0;
}