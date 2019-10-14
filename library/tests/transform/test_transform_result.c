#include "transform/transform_result.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Construction test: a TransformResult holds an owned array-of-
 * NoteEvent-pointers-plus-count -- the shared output shape every
 * concrete Transformation subtype's apply function returns.
 *
 * Invariant test: an empty TransformResult (count == 0) is valid, not
 * an error (a transformation can legitimately produce no output
 * events). A non-empty array with a NULL entry is rejected.
 */

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

static ProvenanceReference *make_present_provenance(const char *target_id_value) {
    ProvenanceReference *provenance = NULL;
    provenance_reference_create_present(make_id(target_id_value), &provenance);
    return provenance;
}

static NoteEvent *make_note_event(const char *id_value, const char *provenance_target_id_value) {
    NoteEvent *note_event = NULL;
    note_event_create(make_id(id_value), make_temporal_anchor(1, 4), make_duration(1, 4), make_pitch("pitch-a"),
                       NULL, NULL, make_present_provenance(provenance_target_id_value), &note_event);
    return note_event;
}

static void test_create_with_events(void) {
    NoteEvent *events[2];
    events[0] = make_note_event("note-1", "source-note");
    events[1] = make_note_event("note-2", "source-note");

    TransformResult *result = NULL;
    TransformResultResult r = transform_result_create(events, 2, &result);
    assert(r == TRANSFORM_RESULT_OK);
    assert(transform_result_event_count(result) == 2);
    assert(transform_result_event_at(result, 0) == events[0]);
    assert(transform_result_event_at(result, 1) == events[1]);

    transform_result_destroy(result);
}

static void test_create_empty_is_valid(void) {
    TransformResult *result = NULL;
    TransformResultResult r = transform_result_create(NULL, 0, &result);
    assert(r == TRANSFORM_RESULT_OK);
    assert(transform_result_event_count(result) == 0);

    transform_result_destroy(result);
}

static void test_create_rejects_null_entry(void) {
    NoteEvent *events[2];
    events[0] = make_note_event("note-1", "source-note");
    events[1] = NULL;

    TransformResult *result = NULL;
    TransformResultResult r = transform_result_create(events, 2, &result);
    assert(r == TRANSFORM_RESULT_ERR_NULL_EVENT);

    note_event_destroy(events[0]);
}

/* Every derived NoteEvent in a TransformResult carries its own
   ProvenanceReference pointing back to its source -- TransformResult
   does not need a separate provenance field of its own. */
static void test_events_carry_their_own_provenance(void) {
    NoteEvent *events[1];
    events[0] = make_note_event("note-1", "source-note");

    TransformResult *result = NULL;
    transform_result_create(events, 1, &result);

    const ProvenanceReference *provenance = note_event_provenance(transform_result_event_at(result, 0));
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);

    EntityID *expected_target = make_id("source-note");
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_target));
    entity_id_destroy(expected_target);

    transform_result_destroy(result);
}

int main(void) {
    test_create_with_events();
    test_create_empty_is_valid();
    test_create_rejects_null_entry();
    test_events_carry_their_own_provenance();
    printf("test_transform_result: ok\n");
    return 0;
}