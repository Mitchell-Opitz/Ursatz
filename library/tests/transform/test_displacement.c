#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/displacement.h"

/*
 * Construction test: Displacement shifts only a TemporalAnchor
 * (declared via direction + non-negative magnitude), leaving every
 * other event field untouched per the base Transformation's
 * preserved_dimensions.
 *
 * Apply test: displacement_apply shifts the event's onset forward or
 * backward by the exact rational magnitude, leaving every other field
 * untouched.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration;
    assert(duration_create(numerator, denominator, &duration) == DURATION_OK);
    return duration;
}

static NoteEvent *make_note_event(const char *id_value, long onset_numerator, unsigned long onset_denominator) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    MusicalTime *position;
    assert(musical_time_create(onset_numerator, onset_denominator, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = make_duration(1, 4);
    PitchIdentity *pitch;
    assert(pitch_identity_create(id_value, strlen(id_value) + 1, &pitch) == PITCH_IDENTITY_OK);

    NoteEvent *note_event;
    assert(note_event_create(id, anchor, duration, pitch, NULL, NULL, make_absent_provenance(), &note_event) ==
           NOTE_EVENT_OK);
    return note_event;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("displacement-delay-eighth-note", &id) == ENTITY_ID_OK);

    Displacement *displacement;
    assert(displacement_create(id, DISPLACEMENT_DIRECTION_FORWARD, make_duration(1, 8), make_absent_provenance(),
                                &displacement) == DISPLACEMENT_OK);

    assert(displacement_direction(displacement) == DISPLACEMENT_DIRECTION_FORWARD);
    assert(duration_numerator(displacement_magnitude(displacement)) == 1);
    assert(duration_denominator(displacement_magnitude(displacement)) == 8);

    const Transformation *base = displacement_transformation(displacement);
    assert(strcmp(transformation_modified_dimensions(base), "temporal position (TemporalAnchor)") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "pitch, duration, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == displacement_id(displacement));

    EntityID *other_id;
    assert(entity_id_create("displacement-anticipate-sixteenth-note", &other_id) == ENTITY_ID_OK);
    Displacement *other;
    assert(displacement_create(other_id, DISPLACEMENT_DIRECTION_BACKWARD, make_duration(1, 16),
                                make_absent_provenance(), &other) == DISPLACEMENT_OK);
    assert(displacement_direction(other) == DISPLACEMENT_DIRECTION_BACKWARD);

    assert(displacement_identity_equals(displacement, displacement));
    assert(!displacement_identity_equals(displacement, other));

    EntityID *bad_id;
    assert(entity_id_create("displacement-rejects-null-magnitude", &bad_id) == ENTITY_ID_OK);
    Displacement *bad;
    assert(displacement_create(bad_id, DISPLACEMENT_DIRECTION_FORWARD, NULL, make_absent_provenance(), &bad) ==
           DISPLACEMENT_ERR_NULL_MAGNITUDE);
    entity_id_destroy(bad_id);

    /* Apply forward: onset 0/4 shifted forward by 1/8 -> 1/8. */
    NoteEvent *event = make_note_event("note-onbeat", 0, 4);
    TransformResult *result;
    assert(displacement_apply(displacement, event, &result) == DISPLACEMENT_APPLY_OK);
    assert(transform_result_event_count(result) == 1);
    const NoteEvent *derived = transform_result_event_at(result, 0);
    assert(musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(derived))) == 1);
    assert(musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(derived))) == 8);
    assert(pitch_identity_equals(note_event_pitch(derived), note_event_pitch(event)));
    transform_result_destroy(result);

    /* Apply backward: onset 4/16 shifted backward by 1/16 -> 3/16. A
       backward shift that would go negative is legal too -- MusicalTime
       allows negative positions (e.g. pickup measures). */
    NoteEvent *event2 = make_note_event("note-anticipated", 4, 16);
    TransformResult *result2;
    assert(displacement_apply(other, event2, &result2) == DISPLACEMENT_APPLY_OK);
    const NoteEvent *derived2 = transform_result_event_at(result2, 0);
    assert(musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(derived2))) == 3);
    assert(musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(derived2))) == 16);

    const ProvenanceReference *provenance = note_event_provenance(derived2);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source;
    assert(entity_id_create("note-anticipated", &expected_source) == ENTITY_ID_OK);
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
    entity_id_destroy(expected_source);

    transform_result_destroy(result2);

    TransformResult *rejected_result = NULL;
    assert(displacement_apply(NULL, event, &rejected_result) == DISPLACEMENT_APPLY_ERR_NULL_DISPLACEMENT);
    assert(displacement_apply(displacement, NULL, &rejected_result) == DISPLACEMENT_APPLY_ERR_NULL_EVENT);
    assert(rejected_result == NULL);

    note_event_destroy(event);
    note_event_destroy(event2);
    displacement_destroy(displacement);
    displacement_destroy(other);

    printf("test_displacement: ok\n");
    return 0;
}