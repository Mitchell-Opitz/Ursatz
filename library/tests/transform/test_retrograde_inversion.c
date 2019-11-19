#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/retrograde_inversion.h"

/*
 * Construction test: RetrogradeInversion composes a Retrograde and an
 * Inversion rather than reimplementing either -- both are reachable
 * from the composed object and each carries the expected values from
 * this constructor's arguments.
 *
 * Apply test: retrograde_inversion_apply calls retrograde_apply then
 * inversion_apply -- it does not reimplement either's arithmetic. Two
 * events are reordered (Retrograde) and each pitch is reflected about
 * the axis (Inversion), verified against the same known values
 * test_retrograde.c and test_inversion.c use for each component alone.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static PitchIdentity *make_axis(const char *token) {
    PitchIdentity *axis;
    assert(pitch_identity_create(token, strlen(token), &axis) == PITCH_IDENTITY_OK);
    return axis;
}

static TuningSystem *make_tuning_system(const char *id_value, const char *name) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    TuningSystem *tuning_system;
    assert(tuning_system_create(id, name, &tuning_system) == TUNING_SYSTEM_OK);
    return tuning_system;
}

static NoteEvent *make_note_event(const char *id_value, long onset_numerator, unsigned long onset_denominator) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    MusicalTime *position;
    assert(musical_time_create(onset_numerator, onset_denominator, &position) == MUSICAL_TIME_OK);
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
    EntityID *id;
    assert(entity_id_create("retrograde-inversion-k1", &id) == ENTITY_ID_OK);

    unsigned mask = RETROGRADE_DIMENSION_EVENT_ORDER;
    RetrogradeInversion *ri;
    assert(retrograde_inversion_create(id, mask, make_axis("pitch-middle-c"), "chromatic pitch space",
                                        "diatonic, preserve letter-name distance from axis", make_absent_provenance(),
                                        &ri) == RETROGRADE_INVERSION_OK);

    const Retrograde *retrograde = retrograde_inversion_retrograde(ri);
    assert(retrograde_reversed_dimensions(retrograde) == mask);

    const Inversion *inversion = retrograde_inversion_inversion(ri);
    assert(strcmp(inversion_pitch_space(inversion), "chromatic pitch space") == 0);
    assert(strcmp(inversion_spelling_policy(inversion), "diatonic, preserve letter-name distance from axis") == 0);

    const Transformation *base = retrograde_inversion_transformation(ri);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == retrograde_inversion_id(ri));

    /* The component ids are distinct from the composed id and from each other. */
    assert(!entity_id_equals(retrograde_id(retrograde), retrograde_inversion_id(ri)));
    assert(!entity_id_equals(inversion_id(inversion), retrograde_inversion_id(ri)));
    assert(!entity_id_equals(retrograde_id(retrograde), inversion_id(inversion)));

    EntityID *other_id;
    assert(entity_id_create("retrograde-inversion-k2", &other_id) == ENTITY_ID_OK);
    RetrogradeInversion *other;
    assert(retrograde_inversion_create(other_id, mask, make_axis("pitch-a4"), "chromatic pitch space",
                                        "diatonic, preserve letter-name distance from axis", make_absent_provenance(),
                                        &other) == RETROGRADE_INVERSION_OK);

    assert(retrograde_inversion_identity_equals(ri, ri));
    assert(!retrograde_inversion_identity_equals(ri, other));

    EntityID *bad_id;
    assert(entity_id_create("retrograde-inversion-rejects-no-dimensions", &bad_id) == ENTITY_ID_OK);
    RetrogradeInversion *bad;
    assert(retrograde_inversion_create(bad_id, 0, make_axis("pitch-middle-c"), "chromatic pitch space", "diatonic",
                                        make_absent_provenance(),
                                        &bad) == RETROGRADE_INVERSION_ERR_NO_DIMENSIONS);
    entity_id_destroy(bad_id);

    /* Apply: same two-event span as test_retrograde.c (onsets 0/4, 1/4,
       durations 1/4) -- reorders to onsets 1/4, 0/1. Pitches 64/1 and
       68/1 are each reflected about axis 60/1 (test_inversion.c's own
       reflection math): 2*60-64=56, 2*60-68=52. */
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");
    PitchNumericValue *axis_value;
    assert(pitch_numeric_value_create(tuning_system, 60, 1, &axis_value) == PITCH_NUMERIC_VALUE_OK);
    PitchNumericValue *pitch_a;
    assert(pitch_numeric_value_create(tuning_system, 64, 1, &pitch_a) == PITCH_NUMERIC_VALUE_OK);
    PitchNumericValue *pitch_b;
    assert(pitch_numeric_value_create(tuning_system, 68, 1, &pitch_b) == PITCH_NUMERIC_VALUE_OK);

    NoteEvent *first = make_note_event("note-first", 0, 4);
    NoteEvent *second = make_note_event("note-second", 1, 4);
    NoteEvent *events[2];
    events[0] = first;
    events[1] = second;
    const PitchNumericValue *pitch_values[2];
    pitch_values[0] = pitch_a;
    pitch_values[1] = pitch_b;

    TransformResult *result;
    assert(retrograde_inversion_apply(ri, events, pitch_values, 2, axis_value, &result) ==
           RETROGRADE_INVERSION_APPLY_OK);
    assert(transform_result_event_count(result) == 2);

    const NoteEvent *derived_first = transform_result_event_at(result, 0);
    const NoteEvent *derived_second = transform_result_event_at(result, 1);

    assert(musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(derived_first))) == 1);
    assert(musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(derived_first))) == 4);
    assert(musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(derived_second))) == 0);
    assert(musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(derived_second))) == 1);

    PitchNumericValue *expected_a;
    assert(pitch_numeric_value_create(tuning_system, 56, 1, &expected_a) == PITCH_NUMERIC_VALUE_OK);
    PitchIdentity *expected_a_identity;
    assert(pitch_numeric_value_to_identity(expected_a, &expected_a_identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_identity_equals(note_event_pitch(derived_first), expected_a_identity));

    PitchNumericValue *expected_b;
    assert(pitch_numeric_value_create(tuning_system, 52, 1, &expected_b) == PITCH_NUMERIC_VALUE_OK);
    PitchIdentity *expected_b_identity;
    assert(pitch_numeric_value_to_identity(expected_b, &expected_b_identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_identity_equals(note_event_pitch(derived_second), expected_b_identity));

    pitch_identity_destroy(expected_a_identity);
    pitch_identity_destroy(expected_b_identity);
    pitch_numeric_value_destroy(expected_a);
    pitch_numeric_value_destroy(expected_b);
    transform_result_destroy(result);

    TransformResult *rejected_result = NULL;
    assert(retrograde_inversion_apply(NULL, events, pitch_values, 2, axis_value, &rejected_result) ==
           RETROGRADE_INVERSION_APPLY_ERR_NULL_RETROGRADE_INVERSION);
    assert(retrograde_inversion_apply(ri, NULL, pitch_values, 2, axis_value, &rejected_result) ==
           RETROGRADE_INVERSION_APPLY_ERR_NULL_EVENTS);
    assert(retrograde_inversion_apply(ri, events, pitch_values, 0, axis_value, &rejected_result) ==
           RETROGRADE_INVERSION_APPLY_ERR_EMPTY);
    assert(retrograde_inversion_apply(ri, events, NULL, 2, axis_value, &rejected_result) ==
           RETROGRADE_INVERSION_APPLY_ERR_NULL_PITCH_VALUES);
    assert(retrograde_inversion_apply(ri, events, pitch_values, 2, NULL, &rejected_result) ==
           RETROGRADE_INVERSION_APPLY_ERR_NULL_AXIS_VALUE);
    assert(rejected_result == NULL);

    /* A dimension besides EVENT_ORDER on the composed Retrograde is forwarded as not-implemented. */
    EntityID *pitch_dim_id;
    assert(entity_id_create("retrograde-inversion-pitch-dim", &pitch_dim_id) == ENTITY_ID_OK);
    RetrogradeInversion *pitch_dim_ri;
    assert(retrograde_inversion_create(pitch_dim_id, RETROGRADE_DIMENSION_PITCH, make_axis("pitch-middle-c"),
                                        "chromatic pitch space", "diatonic, preserve letter-name distance from axis",
                                        make_absent_provenance(), &pitch_dim_ri) == RETROGRADE_INVERSION_OK);
    TransformResult *not_implemented_result = NULL;
    assert(retrograde_inversion_apply(pitch_dim_ri, events, pitch_values, 2, axis_value, &not_implemented_result) ==
           RETROGRADE_INVERSION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED);
    assert(not_implemented_result == NULL);
    retrograde_inversion_destroy(pitch_dim_ri);

    note_event_destroy(first);
    note_event_destroy(second);
    pitch_numeric_value_destroy(axis_value);
    pitch_numeric_value_destroy(pitch_a);
    pitch_numeric_value_destroy(pitch_b);
    tuning_system_destroy(tuning_system);

    retrograde_inversion_destroy(ri);
    retrograde_inversion_destroy(other);

    printf("test_retrograde_inversion: ok\n");
    return 0;
}