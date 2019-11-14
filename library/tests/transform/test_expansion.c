#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/expansion.h"

/*
 * Construction test: Expansion extends a Structure with appended
 * events by reference (owned EntityID pointers) rather than by
 * copying NoteEvent payloads.
 *
 * Apply test: expansion_apply inserts real appended NoteEvents at a
 * declared position within a real source sequence -- the "append at a
 * declared position" scoping decided for this pass. Appended events
 * must match the declared appended_event_ids, in order.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static NoteEvent *make_note_event(const char *id_value) {
    EntityID *id = make_id(id_value);
    MusicalTime *position;
    assert(musical_time_create(0, 4, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration;
    assert(duration_create(1, 4, &duration) == DURATION_OK);
    PitchIdentity *pitch;
    assert(pitch_identity_create(id_value, strlen(id_value) + 1, &pitch) == PITCH_IDENTITY_OK);

    NoteEvent *note_event;
    assert(note_event_create(id, anchor, duration, pitch, NULL, NULL, make_absent_provenance(), &note_event) ==
           NOTE_EVENT_OK);
    return note_event;
}

int main(void) {
    EntityID *id = make_id("expansion-extend-theme-a");
    EntityID *source_structure_id = make_id("structure-theme-a");
    EntityID *event_ids[2] = {make_id("note-event-5"), make_id("note-event-6")};

    Expansion *expansion;
    assert(expansion_create(id, source_structure_id, event_ids, 2, make_absent_provenance(), &expansion) ==
           EXPANSION_OK);

    assert(entity_id_equals(expansion_source_structure_id(expansion), source_structure_id));
    assert(expansion_appended_event_id_count(expansion) == 2);
    assert(strcmp(entity_id_value(expansion_appended_event_id_at(expansion, 0)), "note-event-5") == 0);
    assert(strcmp(entity_id_value(expansion_appended_event_id_at(expansion, 1)), "note-event-6") == 0);

    const Transformation *base = expansion_transformation(expansion);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "event membership (extension)") == 0);
    assert(transformation_id(base) == expansion_id(expansion));

    EntityID *other_id = make_id("expansion-other");
    EntityID *other_source_id = make_id("structure-theme-b");
    EntityID *other_event_ids[1] = {make_id("note-event-7")};
    Expansion *other;
    assert(expansion_create(other_id, other_source_id, other_event_ids, 1, make_absent_provenance(), &other) ==
           EXPANSION_OK);

    assert(expansion_identity_equals(expansion, expansion));
    assert(!expansion_identity_equals(expansion, other));

    EntityID *empty_id = make_id("expansion-rejects-empty");
    EntityID *empty_source_id = make_id("structure-theme-c");
    Expansion *empty;
    assert(expansion_create(empty_id, empty_source_id, NULL, 0, make_absent_provenance(), &empty) ==
           EXPANSION_ERR_NULL_APPENDED_EVENT_IDS);
    entity_id_destroy(empty_id);
    entity_id_destroy(empty_source_id);

    EntityID *null_entry_id = make_id("expansion-rejects-null-entry");
    EntityID *null_entry_source_id = make_id("structure-theme-d");
    EntityID *bad_event_ids[2] = {make_id("note-event-8"), NULL};
    Expansion *null_entry;
    assert(expansion_create(null_entry_id, null_entry_source_id, bad_event_ids, 2, make_absent_provenance(),
                             &null_entry) == EXPANSION_ERR_NULL_APPENDED_EVENT_ID);
    entity_id_destroy(null_entry_id);
    entity_id_destroy(null_entry_source_id);
    entity_id_destroy(bad_event_ids[0]);

    /* Apply: source [note-a, note-b], appended [note-event-5,
       note-event-6] (this fixture's declared ids) inserted at
       position 1 -> [note-a, note-event-5, note-event-6, note-b]. */
    NoteEvent *note_a = make_note_event("note-a");
    NoteEvent *note_b = make_note_event("note-b");
    NoteEvent *source_events[2];
    source_events[0] = note_a;
    source_events[1] = note_b;

    NoteEvent *appended5 = make_note_event("note-event-5");
    NoteEvent *appended6 = make_note_event("note-event-6");
    NoteEvent *appended_events[2];
    appended_events[0] = appended5;
    appended_events[1] = appended6;

    TransformResult *result;
    assert(expansion_apply(expansion, source_events, 2, appended_events, 2, 1, &result) == EXPANSION_APPLY_OK);
    assert(transform_result_event_count(result) == 4);

    EntityID *expected_ids[4];
    expected_ids[0] = make_id("note-a");
    expected_ids[1] = make_id("note-event-5");
    expected_ids[2] = make_id("note-event-6");
    expected_ids[3] = make_id("note-b");
    for (size_t i = 0; i < 4; i++) {
        const ProvenanceReference *provenance = note_event_provenance(transform_result_event_at(result, i));
        assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
        assert(entity_id_equals(provenance_reference_target_id(provenance), expected_ids[i]));
        entity_id_destroy(expected_ids[i]);
    }

    transform_result_destroy(result);

    TransformResult *rejected_result = NULL;
    assert(expansion_apply(NULL, source_events, 2, appended_events, 2, 1, &rejected_result) ==
           EXPANSION_APPLY_ERR_NULL_EXPANSION);
    assert(expansion_apply(expansion, source_events, 2, NULL, 2, 1, &rejected_result) ==
           EXPANSION_APPLY_ERR_NULL_APPENDED_EVENTS);
    assert(expansion_apply(expansion, source_events, 2, appended_events, 1, 1, &rejected_result) ==
           EXPANSION_APPLY_ERR_APPENDED_EVENT_COUNT_MISMATCH);
    assert(expansion_apply(expansion, source_events, 2, appended_events, 2, 3, &rejected_result) ==
           EXPANSION_APPLY_ERR_POSITION_OUT_OF_RANGE);
    assert(rejected_result == NULL);

    /* Appended events that don't match the declared ids, in order, are rejected. */
    NoteEvent *swapped_appended[2];
    swapped_appended[0] = appended6;
    swapped_appended[1] = appended5;
    TransformResult *mismatch_result = NULL;
    assert(expansion_apply(expansion, source_events, 2, swapped_appended, 2, 1, &mismatch_result) ==
           EXPANSION_APPLY_ERR_APPENDED_EVENT_ID_MISMATCH);
    assert(mismatch_result == NULL);

    note_event_destroy(note_a);
    note_event_destroy(note_b);
    note_event_destroy(appended5);
    note_event_destroy(appended6);

    expansion_destroy(expansion);
    expansion_destroy(other);

    printf("test_expansion: ok\n");
    return 0;
}