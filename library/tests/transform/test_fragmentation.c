#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/fragmentation.h"

/*
 * Construction test: Fragmentation extracts events by reference (owned
 * EntityID pointers) rather than by copying NoteEvent payloads --
 * there is no field here of NoteEvent-shaped content, only ids.
 *
 * Apply test: fragmentation_apply looks up the declared
 * extracted_event_ids against a real source sequence and clones the
 * matches, in the declared order. A declared id missing from the
 * source is a real error.
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
    EntityID *id = make_id("fragmentation-opening-motif");
    EntityID *source_structure_id = make_id("structure-theme-a");
    EntityID *event_ids[2] = {make_id("note-event-1"), make_id("note-event-2")};

    Fragmentation *fragmentation;
    assert(fragmentation_create(id, source_structure_id, event_ids, 2, make_absent_provenance(), &fragmentation) ==
           FRAGMENTATION_OK);

    assert(entity_id_equals(fragmentation_source_structure_id(fragmentation), source_structure_id));
    assert(fragmentation_extracted_event_id_count(fragmentation) == 2);
    assert(strcmp(entity_id_value(fragmentation_extracted_event_id_at(fragmentation, 0)), "note-event-1") == 0);
    assert(strcmp(entity_id_value(fragmentation_extracted_event_id_at(fragmentation, 1)), "note-event-2") == 0);

    const Transformation *base = fragmentation_transformation(fragmentation);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "event membership (subset extraction)") == 0);
    assert(transformation_id(base) == fragmentation_id(fragmentation));

    EntityID *other_id = make_id("fragmentation-other");
    EntityID *other_source_id = make_id("structure-theme-b");
    EntityID *other_event_ids[1] = {make_id("note-event-3")};
    Fragmentation *other;
    assert(fragmentation_create(other_id, other_source_id, other_event_ids, 1, make_absent_provenance(), &other) ==
           FRAGMENTATION_OK);

    assert(fragmentation_identity_equals(fragmentation, fragmentation));
    assert(!fragmentation_identity_equals(fragmentation, other));

    EntityID *empty_id = make_id("fragmentation-rejects-empty");
    EntityID *empty_source_id = make_id("structure-theme-c");
    Fragmentation *empty;
    assert(fragmentation_create(empty_id, empty_source_id, NULL, 0, make_absent_provenance(), &empty) ==
           FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_IDS);
    entity_id_destroy(empty_id);
    entity_id_destroy(empty_source_id);

    EntityID *null_entry_id = make_id("fragmentation-rejects-null-entry");
    EntityID *null_entry_source_id = make_id("structure-theme-d");
    EntityID *bad_event_ids[2] = {make_id("note-event-4"), NULL};
    Fragmentation *null_entry;
    assert(fragmentation_create(null_entry_id, null_entry_source_id, bad_event_ids, 2, make_absent_provenance(),
                                 &null_entry) == FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_ID);
    entity_id_destroy(null_entry_id);
    entity_id_destroy(null_entry_source_id);
    entity_id_destroy(bad_event_ids[0]);

    /* Apply: source has three events; fragmentation declares
       note-event-1, note-event-2 (this fixture's own ids). */
    NoteEvent *event1 = make_note_event("note-event-1");
    NoteEvent *event2 = make_note_event("note-event-2");
    NoteEvent *event3 = make_note_event("note-event-unrelated");
    NoteEvent *source_events[3];
    source_events[0] = event3;
    source_events[1] = event1;
    source_events[2] = event2;

    TransformResult *result;
    assert(fragmentation_apply(fragmentation, source_events, 3, &result) == FRAGMENTATION_APPLY_OK);
    assert(transform_result_event_count(result) == 2);

    const NoteEvent *derived0 = transform_result_event_at(result, 0);
    const NoteEvent *derived1 = transform_result_event_at(result, 1);
    EntityID *expected0 = make_id("note-event-1");
    EntityID *expected1 = make_id("note-event-2");
    const ProvenanceReference *provenance0 = note_event_provenance(derived0);
    const ProvenanceReference *provenance1 = note_event_provenance(derived1);
    assert(provenance_reference_state(provenance0) == PROVENANCE_REFERENCE_PRESENT);
    assert(entity_id_equals(provenance_reference_target_id(provenance0), expected0));
    assert(entity_id_equals(provenance_reference_target_id(provenance1), expected1));
    entity_id_destroy(expected0);
    entity_id_destroy(expected1);

    transform_result_destroy(result);

    TransformResult *rejected_result = NULL;
    assert(fragmentation_apply(NULL, source_events, 3, &rejected_result) ==
           FRAGMENTATION_APPLY_ERR_NULL_FRAGMENTATION);
    assert(fragmentation_apply(fragmentation, NULL, 3, &rejected_result) ==
           FRAGMENTATION_APPLY_ERR_NULL_SOURCE_EVENTS);
    assert(rejected_result == NULL);

    /* A declared extraction target missing from the source is an error. */
    NoteEvent *lone_source[1];
    lone_source[0] = event3;
    TransformResult *not_found_result = NULL;
    assert(fragmentation_apply(fragmentation, lone_source, 1, &not_found_result) ==
           FRAGMENTATION_APPLY_ERR_EVENT_NOT_FOUND);
    assert(not_found_result == NULL);

    note_event_destroy(event1);
    note_event_destroy(event2);
    note_event_destroy(event3);

    fragmentation_destroy(fragmentation);
    fragmentation_destroy(other);

    printf("test_fragmentation: ok\n");
    return 0;
}