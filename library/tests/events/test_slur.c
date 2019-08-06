#include "events/slur.h"

#include "events/note_event.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

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

static void test_create_rejects_null_ids(void) {
    Slur *slur = NULL;
    SlurResult r = slur_create(NULL, 2, &slur);
    assert(r == SLUR_ERR_NULL_NOTE_EVENT_IDS);
    assert(slur == NULL);
}

static void test_create_rejects_too_few_ids(void) {
    EntityID *ids[1] = {make_id("note-1")};

    Slur *slur = NULL;
    SlurResult r = slur_create(ids, 1, &slur);
    assert(r == SLUR_ERR_TOO_FEW_NOTE_EVENT_IDS);
    assert(slur == NULL);

    entity_id_destroy(ids[0]);
}

static void test_create_rejects_null_entry(void) {
    EntityID *ids[2] = {make_id("note-1"), NULL};

    Slur *slur = NULL;
    SlurResult r = slur_create(ids, 2, &slur);
    assert(r == SLUR_ERR_NULL_NOTE_EVENT_ID);
    assert(slur == NULL);

    entity_id_destroy(ids[0]);
}

static void test_create_accepts_valid(void) {
    EntityID *ids[3] = {make_id("note-1"), make_id("note-2"), make_id("note-3")};

    Slur *slur = NULL;
    SlurResult r = slur_create(ids, 3, &slur);
    assert(r == SLUR_OK);
    assert(slur != NULL);
    assert(slur_note_event_id_count(slur) == 3);

    EntityID *expected_1 = make_id("note-1");
    EntityID *expected_2 = make_id("note-2");
    EntityID *expected_3 = make_id("note-3");
    assert(entity_id_equals(slur_note_event_id_at(slur, 0), expected_1));
    assert(entity_id_equals(slur_note_event_id_at(slur, 1), expected_2));
    assert(entity_id_equals(slur_note_event_id_at(slur, 2), expected_3));
    entity_id_destroy(expected_1);
    entity_id_destroy(expected_2);
    entity_id_destroy(expected_3);

    slur_destroy(slur);
}

/* Relationship: constructing a Slur over a range of NoteEvents does not
   alter any spanned NoteEvent's own fields. Slur only ever takes
   EntityIDs, never the NoteEvent pointers themselves, so there is no
   path from slur_create back into any NoteEvent's storage. */
static void test_slur_does_not_alter_spanned_note_events(void) {
    NoteEvent *first = make_note_event("note-1");
    NoteEvent *second = make_note_event("note-2");
    NoteEvent *third = make_note_event("note-3");

    NoteEvent *first_before = make_note_event("note-1");
    NoteEvent *second_before = make_note_event("note-2");
    NoteEvent *third_before = make_note_event("note-3");
    assert(note_event_equals_by_value(first, first_before));
    assert(note_event_equals_by_value(second, second_before));
    assert(note_event_equals_by_value(third, third_before));

    EntityID *ids[3] = {make_id(entity_id_value(note_event_id(first))),
                         make_id(entity_id_value(note_event_id(second))),
                         make_id(entity_id_value(note_event_id(third)))};

    Slur *slur = NULL;
    slur_create(ids, 3, &slur);

    assert(note_event_equals_by_id(first, first_before));
    assert(note_event_equals_by_value(first, first_before));
    assert(note_event_equals_by_id(second, second_before));
    assert(note_event_equals_by_value(second, second_before));
    assert(note_event_equals_by_id(third, third_before));
    assert(note_event_equals_by_value(third, third_before));

    slur_destroy(slur);
    note_event_destroy(first);
    note_event_destroy(second);
    note_event_destroy(third);
    note_event_destroy(first_before);
    note_event_destroy(second_before);
    note_event_destroy(third_before);
}

int main(void) {
    test_create_rejects_null_ids();
    test_create_rejects_too_few_ids();
    test_create_rejects_null_entry();
    test_create_accepts_valid();
    test_slur_does_not_alter_spanned_note_events();
    printf("test_slur: ok\n");
    return 0;
}