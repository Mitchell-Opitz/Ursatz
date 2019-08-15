#include "events/tie.h"

#include "events/note_event.h"

#include <assert.h>
#include <stddef.h>

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    entity_id_create(value, &id);
    return id;
}

static TemporalAnchor *make_temporal_anchor(long numerator, unsigned long denominator) {
    MusicalTime *position = NULL;
    musical_time_create(numerator, denominator, &position);
    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);
    return anchor;
}

static Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration = NULL;
    duration_create(numerator, denominator, &duration);
    return duration;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *pitch = NULL;
    pitch_identity_create(token, 8, &pitch);
    return pitch;
}

static NoteEvent *make_note_event(const char *id_value) {
    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    NoteEvent *note_event = NULL;
    note_event_create(make_id(id_value), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, provenance, &note_event);
    return note_event;
}

/* Both failure paths below return before tie_create takes ownership of
   anything, so the valid co-argument in each call is still owned by the
   caller and must be destroyed here (same pattern as
   tests/events/test_note_event.c). */

static void test_create_rejects_null_first_id(void) {
    EntityID *second_id = make_id("note-2");

    Tie *tie = NULL;
    TieResult r = tie_create(NULL, second_id, &tie);
    assert(r == TIE_ERR_NULL_FIRST_NOTE_EVENT_ID);
    assert(tie == NULL);

    entity_id_destroy(second_id);
}

static void test_create_rejects_null_second_id(void) {
    EntityID *first_id = make_id("note-1");

    Tie *tie = NULL;
    TieResult r = tie_create(first_id, NULL, &tie);
    assert(r == TIE_ERR_NULL_SECOND_NOTE_EVENT_ID);
    assert(tie == NULL);

    entity_id_destroy(first_id);
}

static void test_create_accepts_valid(void) {
    Tie *tie = NULL;
    TieResult r = tie_create(make_id("note-1"), make_id("note-2"), &tie);
    assert(r == TIE_OK);
    assert(tie != NULL);

    EntityID *expected_first = make_id("note-1");
    EntityID *expected_second = make_id("note-2");
    assert(entity_id_equals(tie_first_note_event_id(tie), expected_first));
    assert(entity_id_equals(tie_second_note_event_id(tie), expected_second));
    entity_id_destroy(expected_first);
    entity_id_destroy(expected_second);

    tie_destroy(tie);
}

/* Relationship: constructing a Tie between two NoteEvents does not alter
   either NoteEvent's own fields. Tie only ever takes EntityIDs, never
   the NoteEvent pointers themselves, so there is no path from
   tie_create back into either NoteEvent's storage. */
static void test_tie_does_not_alter_referenced_note_events(void) {
    NoteEvent *first = make_note_event("note-1");
    NoteEvent *second = make_note_event("note-2");

    NoteEvent *first_before = make_note_event("note-1");
    NoteEvent *second_before = make_note_event("note-2");
    assert(note_event_equals_by_value(first, first_before));
    assert(note_event_equals_by_value(second, second_before));

    EntityID *first_id = make_id(entity_id_value(note_event_id(first)));
    EntityID *second_id = make_id(entity_id_value(note_event_id(second)));

    Tie *tie = NULL;
    tie_create(first_id, second_id, &tie);

    assert(note_event_equals_by_id(first, first_before));
    assert(note_event_equals_by_value(first, first_before));
    assert(note_event_equals_by_id(second, second_before));
    assert(note_event_equals_by_value(second, second_before));

    tie_destroy(tie);
    note_event_destroy(first);
    note_event_destroy(second);
    note_event_destroy(first_before);
    note_event_destroy(second_before);
}

int main(void) {
    test_create_rejects_null_first_id();
    test_create_rejects_null_second_id();
    test_create_accepts_valid();
    test_tie_does_not_alter_referenced_note_events();
    return 0;
}