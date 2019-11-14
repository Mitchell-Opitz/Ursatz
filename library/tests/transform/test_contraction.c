#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/contraction.h"

/*
 * Construction test: Contraction compresses a Structure by dropping
 * events by reference (owned EntityID pointers) rather than by
 * copying NoteEvent payloads.
 *
 * Apply test: contraction_apply clones every source event whose id is
 * NOT declared removed, preserving relative order. A declared removal
 * target missing from the source is a real error.
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
    EntityID *id = make_id("contraction-trim-theme-a");
    EntityID *source_structure_id = make_id("structure-theme-a");
    EntityID *event_ids[2] = {make_id("note-event-9"), make_id("note-event-10")};

    Contraction *contraction;
    assert(contraction_create(id, source_structure_id, event_ids, 2, make_absent_provenance(), &contraction) ==
           CONTRACTION_OK);

    assert(entity_id_equals(contraction_source_structure_id(contraction), source_structure_id));
    assert(contraction_removed_event_id_count(contraction) == 2);
    assert(strcmp(entity_id_value(contraction_removed_event_id_at(contraction, 0)), "note-event-9") == 0);
    assert(strcmp(entity_id_value(contraction_removed_event_id_at(contraction, 1)), "note-event-10") == 0);

    const Transformation *base = contraction_transformation(contraction);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "event membership (compression)") == 0);
    assert(transformation_id(base) == contraction_id(contraction));

    EntityID *other_id = make_id("contraction-other");
    EntityID *other_source_id = make_id("structure-theme-b");
    EntityID *other_event_ids[1] = {make_id("note-event-11")};
    Contraction *other;
    assert(contraction_create(other_id, other_source_id, other_event_ids, 1, make_absent_provenance(), &other) ==
           CONTRACTION_OK);

    assert(contraction_identity_equals(contraction, contraction));
    assert(!contraction_identity_equals(contraction, other));

    EntityID *empty_id = make_id("contraction-rejects-empty");
    EntityID *empty_source_id = make_id("structure-theme-c");
    Contraction *empty;
    assert(contraction_create(empty_id, empty_source_id, NULL, 0, make_absent_provenance(), &empty) ==
           CONTRACTION_ERR_NULL_REMOVED_EVENT_IDS);
    entity_id_destroy(empty_id);
    entity_id_destroy(empty_source_id);

    EntityID *null_entry_id = make_id("contraction-rejects-null-entry");
    EntityID *null_entry_source_id = make_id("structure-theme-d");
    EntityID *bad_event_ids[2] = {make_id("note-event-12"), NULL};
    Contraction *null_entry;
    assert(contraction_create(null_entry_id, null_entry_source_id, bad_event_ids, 2, make_absent_provenance(),
                               &null_entry) == CONTRACTION_ERR_NULL_REMOVED_EVENT_ID);
    entity_id_destroy(null_entry_id);
    entity_id_destroy(null_entry_source_id);
    entity_id_destroy(bad_event_ids[0]);

    /* Apply: source [note-event-9, note-keep, note-event-10];
       contraction declares note-event-9, note-event-10 removed (this
       fixture's own ids) -> only note-keep remains. */
    NoteEvent *removed1 = make_note_event("note-event-9");
    NoteEvent *keep = make_note_event("note-keep");
    NoteEvent *removed2 = make_note_event("note-event-10");
    NoteEvent *source_events[3];
    source_events[0] = removed1;
    source_events[1] = keep;
    source_events[2] = removed2;

    TransformResult *result;
    assert(contraction_apply(contraction, source_events, 3, &result) == CONTRACTION_APPLY_OK);
    assert(transform_result_event_count(result) == 1);

    const ProvenanceReference *provenance = note_event_provenance(transform_result_event_at(result, 0));
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source = make_id("note-keep");
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
    entity_id_destroy(expected_source);

    transform_result_destroy(result);

    TransformResult *rejected_result = NULL;
    assert(contraction_apply(NULL, source_events, 3, &rejected_result) == CONTRACTION_APPLY_ERR_NULL_CONTRACTION);
    assert(contraction_apply(contraction, NULL, 3, &rejected_result) == CONTRACTION_APPLY_ERR_NULL_SOURCE_EVENTS);
    assert(rejected_result == NULL);

    /* A declared removal target missing from the source is an error. */
    NoteEvent *lone_source[1];
    lone_source[0] = keep;
    TransformResult *not_found_result = NULL;
    assert(contraction_apply(contraction, lone_source, 1, &not_found_result) == CONTRACTION_APPLY_ERR_EVENT_NOT_FOUND);
    assert(not_found_result == NULL);

    /* Removing every event is valid -- an empty result, not an error. */
    EntityID *remove_all_id = make_id("contraction-remove-all");
    EntityID *remove_all_source_id = make_id("structure-theme-e");
    EntityID *remove_all_event_ids[1] = {make_id("note-solo")};
    Contraction *remove_all;
    assert(contraction_create(remove_all_id, remove_all_source_id, remove_all_event_ids, 1, make_absent_provenance(),
                               &remove_all) == CONTRACTION_OK);
    NoteEvent *solo = make_note_event("note-solo");
    NoteEvent *solo_source[1];
    solo_source[0] = solo;
    TransformResult *empty_result;
    assert(contraction_apply(remove_all, solo_source, 1, &empty_result) == CONTRACTION_APPLY_OK);
    assert(transform_result_event_count(empty_result) == 0);
    transform_result_destroy(empty_result);
    note_event_destroy(solo);
    contraction_destroy(remove_all);

    note_event_destroy(removed1);
    note_event_destroy(keep);
    note_event_destroy(removed2);

    contraction_destroy(contraction);
    contraction_destroy(other);

    printf("test_contraction: ok\n");
    return 0;
}