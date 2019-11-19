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
#include "transform/inversion.h"

/*
 * Construction test: an Inversion declares an explicit axis pitch
 * value, pitch space, and spelling policy now, and requires tuning
 * semantics to be explicitly declared absent (TDD §142) rather than
 * silently omitted or implicitly derived from input material.
 *
 * Apply test: inversion_apply reflects a caller-supplied
 * PitchNumericValue about a caller-supplied axis PitchNumericValue
 * (2*axis - value), packaging the result into the output NoteEvent's
 * pitch, exact rational arithmetic throughout (Law 19). A TuningSystem
 * mismatch between the two caller-supplied values is rejected.
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

static NoteEvent *make_note_event(const char *id_value) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
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
    EntityID *id;
    assert(entity_id_create("inversion-about-middle-c", &id) == ENTITY_ID_OK);

    PitchIdentity *axis = make_axis("pitch-middle-c");

    Inversion *inversion;
    assert(inversion_create(id, axis, "chromatic pitch space", "diatonic, preserve letter-name distance from axis",
                             false, make_absent_provenance(), &inversion) == INVERSION_OK);

    assert(pitch_identity_equals(inversion_axis(inversion), axis));
    assert(strcmp(inversion_pitch_space(inversion), "chromatic pitch space") == 0);
    assert(strcmp(inversion_spelling_policy(inversion), "diatonic, preserve letter-name distance from axis") == 0);
    assert(!inversion_has_tuning_semantics(inversion));

    const Transformation *base = inversion_transformation(inversion);
    assert(strcmp(transformation_input_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == inversion_id(inversion));

    EntityID *other_id;
    assert(entity_id_create("inversion-about-a4", &other_id) == ENTITY_ID_OK);
    Inversion *other;
    assert(inversion_create(other_id, make_axis("pitch-a4"), "chromatic pitch space",
                             "diatonic, preserve letter-name distance from axis", false, make_absent_provenance(),
                             &other) == INVERSION_OK);

    assert(inversion_identity_equals(inversion, inversion));
    assert(!inversion_identity_equals(inversion, other));

    EntityID *rejected_id;
    assert(entity_id_create("inversion-rejects-tuning", &rejected_id) == ENTITY_ID_OK);
    Inversion *rejected;
    assert(inversion_create(rejected_id, make_axis("pitch-middle-c"), "chromatic pitch space", "diatonic", true,
                             make_absent_provenance(), &rejected) == INVERSION_ERR_TUNING_SEMANTICS_NOT_SUPPORTED);
    entity_id_destroy(rejected_id);

    /* Apply: reflecting 64/1 about axis 60/1 -> 2*60 - 64 = 56/1. */
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");
    PitchNumericValue *axis_value;
    assert(pitch_numeric_value_create(tuning_system, 60, 1, &axis_value) == PITCH_NUMERIC_VALUE_OK);
    PitchNumericValue *event_pitch_value;
    assert(pitch_numeric_value_create(tuning_system, 64, 1, &event_pitch_value) == PITCH_NUMERIC_VALUE_OK);

    NoteEvent *event = make_note_event("note-e-above-middle-c");
    TransformResult *result;
    assert(inversion_apply(inversion, event, event_pitch_value, axis_value, &result) == INVERSION_APPLY_OK);
    assert(transform_result_event_count(result) == 1);

    const NoteEvent *derived = transform_result_event_at(result, 0);
    PitchNumericValue *expected;
    assert(pitch_numeric_value_create(tuning_system, 56, 1, &expected) == PITCH_NUMERIC_VALUE_OK);
    PitchIdentity *expected_identity;
    assert(pitch_numeric_value_to_identity(expected, &expected_identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_identity_equals(note_event_pitch(derived), expected_identity));

    const ProvenanceReference *provenance = note_event_provenance(derived);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source;
    assert(entity_id_create("note-e-above-middle-c", &expected_source) == ENTITY_ID_OK);
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
    entity_id_destroy(expected_source);

    pitch_identity_destroy(expected_identity);
    pitch_numeric_value_destroy(expected);
    transform_result_destroy(result);

    /* A TuningSystem mismatch between event_pitch_value and axis_value is rejected. */
    TuningSystem *other_tuning_system = make_tuning_system("tuning-24tet", "24-TET");
    PitchNumericValue *mismatched_axis;
    assert(pitch_numeric_value_create(other_tuning_system, 60, 1, &mismatched_axis) == PITCH_NUMERIC_VALUE_OK);
    TransformResult *mismatch_result = NULL;
    assert(inversion_apply(inversion, event, event_pitch_value, mismatched_axis, &mismatch_result) ==
           INVERSION_APPLY_ERR_TUNING_SYSTEM_MISMATCH);
    assert(mismatch_result == NULL);
    pitch_numeric_value_destroy(mismatched_axis);
    tuning_system_destroy(other_tuning_system);

    TransformResult *rejected_result = NULL;
    assert(inversion_apply(NULL, event, event_pitch_value, axis_value, &rejected_result) ==
           INVERSION_APPLY_ERR_NULL_INVERSION);
    assert(inversion_apply(inversion, NULL, event_pitch_value, axis_value, &rejected_result) ==
           INVERSION_APPLY_ERR_NULL_EVENT);
    assert(inversion_apply(inversion, event, NULL, axis_value, &rejected_result) ==
           INVERSION_APPLY_ERR_NULL_PITCH_VALUE);
    assert(inversion_apply(inversion, event, event_pitch_value, NULL, &rejected_result) ==
           INVERSION_APPLY_ERR_NULL_AXIS_VALUE);
    assert(rejected_result == NULL);

    note_event_destroy(event);
    pitch_numeric_value_destroy(axis_value);
    pitch_numeric_value_destroy(event_pitch_value);
    tuning_system_destroy(tuning_system);

    inversion_destroy(inversion);
    inversion_destroy(other);

    printf("test_inversion: ok\n");
    return 0;
}