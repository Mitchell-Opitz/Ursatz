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
#include "transform/transposition.h"

/*
 * Construction test: a Transposition declares pitch displacement
 * (semitones) and spelling behavior now, and requires tuning
 * behavior/range policy/instrument context to be explicitly declared
 * absent (TDD §141) rather than silently omitted.
 *
 * Apply test: transposition_apply shifts a caller-supplied
 * PitchNumericValue by transposition_semitones() and packages the
 * result into the output NoteEvent's pitch, using exact rational
 * arithmetic (Law 19).
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static TuningSystem *make_tuning_system(void) {
    EntityID *id;
    assert(entity_id_create("tuning-12tet", &id) == ENTITY_ID_OK);
    TuningSystem *tuning_system;
    assert(tuning_system_create(id, "12-TET", &tuning_system) == TUNING_SYSTEM_OK);
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
    assert(entity_id_create("transposition-up-major-third", &id) == ENTITY_ID_OK);

    Transposition *transposition;
    assert(transposition_create(id, 4, "diatonic, preserve letter-name interval", false, false, false,
                                 make_absent_provenance(), &transposition) == TRANSPOSITION_OK);

    assert(transposition_semitones(transposition) == 4);
    assert(strcmp(transposition_spelling_behavior(transposition), "diatonic, preserve letter-name interval") == 0);
    assert(!transposition_has_tuning_behavior(transposition));
    assert(!transposition_has_range_policy(transposition));
    assert(!transposition_has_instrument_context(transposition));

    const Transformation *base = transposition_transformation(transposition);
    assert(strcmp(transformation_input_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_output_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == transposition_id(transposition));

    EntityID *other_id;
    assert(entity_id_create("transposition-down-perfect-fifth", &other_id) == ENTITY_ID_OK);
    Transposition *other;
    assert(transposition_create(other_id, -7, "diatonic, preserve letter-name interval", false, false, false,
                                 make_absent_provenance(), &other) == TRANSPOSITION_OK);

    assert(transposition_identity_equals(transposition, transposition));
    assert(!transposition_identity_equals(transposition, other));

    EntityID *rejected_id;
    assert(entity_id_create("transposition-rejects-tuning", &rejected_id) == ENTITY_ID_OK);
    Transposition *rejected;
    assert(transposition_create(rejected_id, 1, "diatonic", true, false, false, make_absent_provenance(),
                                 &rejected) == TRANSPOSITION_ERR_TUNING_BEHAVIOR_NOT_SUPPORTED);
    entity_id_destroy(rejected_id);

    /* Apply: middle C (60/1) up a major third (4 semitones) -> 64/1. */
    TuningSystem *tuning_system = make_tuning_system();
    PitchNumericValue *middle_c;
    assert(pitch_numeric_value_create(tuning_system, 60, 1, &middle_c) == PITCH_NUMERIC_VALUE_OK);

    NoteEvent *event = make_note_event("note-middle-c");
    TransformResult *result;
    assert(transposition_apply(transposition, event, middle_c, &result) == TRANSPOSITION_APPLY_OK);
    assert(transform_result_event_count(result) == 1);

    const NoteEvent *derived = transform_result_event_at(result, 0);
    PitchNumericValue *expected;
    assert(pitch_numeric_value_create(tuning_system, 64, 1, &expected) == PITCH_NUMERIC_VALUE_OK);
    PitchIdentity *expected_identity;
    assert(pitch_numeric_value_to_identity(expected, &expected_identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_identity_equals(note_event_pitch(derived), expected_identity));

    const ProvenanceReference *provenance = note_event_provenance(derived);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source;
    assert(entity_id_create("note-middle-c", &expected_source) == ENTITY_ID_OK);
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
    entity_id_destroy(expected_source);

    pitch_identity_destroy(expected_identity);
    pitch_numeric_value_destroy(expected);
    transform_result_destroy(result);

    TransformResult *rejected_result = NULL;
    assert(transposition_apply(NULL, event, middle_c, &rejected_result) == TRANSPOSITION_APPLY_ERR_NULL_TRANSPOSITION);
    assert(transposition_apply(transposition, NULL, middle_c, &rejected_result) == TRANSPOSITION_APPLY_ERR_NULL_EVENT);
    assert(transposition_apply(transposition, event, NULL, &rejected_result) ==
           TRANSPOSITION_APPLY_ERR_NULL_PITCH_VALUE);
    assert(rejected_result == NULL);

    note_event_destroy(event);
    pitch_numeric_value_destroy(middle_c);
    tuning_system_destroy(tuning_system);

    transposition_destroy(transposition);
    transposition_destroy(other);

    printf("test_transposition: ok\n");
    return 0;
}