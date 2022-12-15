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
#include "transform/intervallic_manipulation.h"

/*
 * Construction test: an IntervallicManipulation owns a copy of its per-interval adjustments
 * and declares which dimensions it keeps and changes.
 *
 * Apply test: each interval is widened or narrowed in its own direction, the change carries
 * on to later notes, rhythm and order are kept, and each output event points back to its
 * input through provenance.
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

static NoteEvent *make_note_event(const char *id_value, long onset_quarters) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    MusicalTime *position;
    assert(musical_time_create(onset_quarters, 4, &position) == MUSICAL_TIME_OK);
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

static IntervallicManipulation *make_manipulation(const char *id_value, const long *adjustments, size_t count) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    IntervallicManipulation *manipulation;
    assert(intervallic_manipulation_create(id, adjustments, count, make_absent_provenance(), &manipulation) ==
           INTERVALLIC_MANIPULATION_OK);
    return manipulation;
}

static void assert_pitch_is(const NoteEvent *event, const TuningSystem *tuning_system, long semitone) {
    PitchNumericValue *expected;
    assert(pitch_numeric_value_create(tuning_system, semitone, 1, &expected) == PITCH_NUMERIC_VALUE_OK);
    PitchIdentity *expected_identity;
    assert(pitch_numeric_value_to_identity(expected, &expected_identity) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_identity_equals(note_event_pitch(event), expected_identity));
    pitch_identity_destroy(expected_identity);
    pitch_numeric_value_destroy(expected);
}

static void test_create(void) {
    long adjustments[2] = {1, -1};
    IntervallicManipulation *manipulation = make_manipulation("intervallic-1", adjustments, 2);

    assert(intervallic_manipulation_adjustment_count(manipulation) == 2);
    assert(intervallic_manipulation_adjustment_at(manipulation, 0) == 1);
    assert(intervallic_manipulation_adjustment_at(manipulation, 1) == -1);

    const Transformation *base = intervallic_manipulation_transformation(manipulation);
    assert(strcmp(transformation_input_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_output_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch (interval sizes)") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == intervallic_manipulation_id(manipulation));

    IntervallicManipulation *other = make_manipulation("intervallic-2", adjustments, 2);
    assert(intervallic_manipulation_identity_equals(manipulation, manipulation));
    assert(!intervallic_manipulation_identity_equals(manipulation, other));
    assert(!intervallic_manipulation_identity_equals(NULL, manipulation));

    intervallic_manipulation_destroy(manipulation);
    intervallic_manipulation_destroy(other);
}

static void test_create_rejects_invalid(void) {
    long adjustments[1] = {1};
    IntervallicManipulation *manipulation = NULL;

    assert(intervallic_manipulation_create(NULL, adjustments, 1, make_absent_provenance(), &manipulation) ==
           INTERVALLIC_MANIPULATION_ERR_NULL_ID);

    EntityID *id;
    assert(entity_id_create("rejected", &id) == ENTITY_ID_OK);
    assert(intervallic_manipulation_create(id, NULL, 1, make_absent_provenance(), &manipulation) ==
           INTERVALLIC_MANIPULATION_ERR_NULL_ADJUSTMENTS);
    assert(intervallic_manipulation_create(id, adjustments, 0, make_absent_provenance(), &manipulation) ==
           INTERVALLIC_MANIPULATION_ERR_NULL_ADJUSTMENTS);
    assert(intervallic_manipulation_create(id, adjustments, 1, NULL, &manipulation) ==
           INTERVALLIC_MANIPULATION_ERR_NULL_PROVENANCE);
    assert(manipulation == NULL);
    entity_id_destroy(id);
}

/* C D C widened by one semitone each: C D+1 C -> 60 63 60 (up a minor third, down a minor third). */
static void test_apply_widens_in_each_direction(void) {
    TuningSystem *tuning_system = make_tuning_system();
    long adjustments[2] = {1, 1};
    IntervallicManipulation *manipulation = make_manipulation("intervallic-widen", adjustments, 2);

    long semitones[3] = {60, 62, 60};
    PitchNumericValue *values[3];
    NoteEvent *events[3];
    const char *ids[3] = {"note-1", "note-2", "note-3"};
    for (int i = 0; i < 3; i++) {
        assert(pitch_numeric_value_create(tuning_system, semitones[i], 1, &values[i]) == PITCH_NUMERIC_VALUE_OK);
        events[i] = make_note_event(ids[i], i);
    }

    TransformResult *result;
    assert(intervallic_manipulation_apply(manipulation, events, (const PitchNumericValue **)values, 3, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_OK);
    assert(transform_result_event_count(result) == 3);
    assert_pitch_is(transform_result_event_at(result, 0), tuning_system, 60);
    assert_pitch_is(transform_result_event_at(result, 1), tuning_system, 63);
    assert_pitch_is(transform_result_event_at(result, 2), tuning_system, 60);

    for (int i = 0; i < 3; i++) {
        const NoteEvent *derived = transform_result_event_at(result, i);
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(derived));
        /* onset is i quarters; MusicalTime is stored reduced, so compare cross-multiplied */
        assert(musical_time_numerator(position) * 4 == (long)i * (long)musical_time_denominator(position));
        assert(duration_numerator(note_event_duration(derived)) == 1);
        assert(duration_denominator(note_event_duration(derived)) == 4);

        const ProvenanceReference *provenance = note_event_provenance(derived);
        assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
        EntityID *expected_source;
        assert(entity_id_create(ids[i], &expected_source) == ENTITY_ID_OK);
        assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
        entity_id_destroy(expected_source);
    }

    transform_result_destroy(result);
    for (int i = 0; i < 3; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    intervallic_manipulation_destroy(manipulation);
    tuning_system_destroy(tuning_system);
}

/* C D E D with {2, -1, 0}: intervals +2 +2 -2 become +4 +1 -2 -> 60 64 65 63. */
static void test_apply_narrows_and_carries_on(void) {
    TuningSystem *tuning_system = make_tuning_system();
    long adjustments[3] = {2, -1, 0};
    IntervallicManipulation *manipulation = make_manipulation("intervallic-mixed", adjustments, 3);

    long semitones[4] = {60, 62, 64, 62};
    long expected[4] = {60, 64, 65, 63};
    PitchNumericValue *values[4];
    NoteEvent *events[4];
    const char *ids[4] = {"note-1", "note-2", "note-3", "note-4"};
    for (int i = 0; i < 4; i++) {
        assert(pitch_numeric_value_create(tuning_system, semitones[i], 1, &values[i]) == PITCH_NUMERIC_VALUE_OK);
        events[i] = make_note_event(ids[i], i);
    }

    TransformResult *result;
    assert(intervallic_manipulation_apply(manipulation, events, (const PitchNumericValue **)values, 4, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_OK);
    assert(transform_result_event_count(result) == 4);
    for (size_t i = 0; i < 4; i++) {
        assert_pitch_is(transform_result_event_at(result, i), tuning_system, expected[i]);
    }

    transform_result_destroy(result);
    for (int i = 0; i < 4; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    intervallic_manipulation_destroy(manipulation);
    tuning_system_destroy(tuning_system);
}

static void test_apply_rejects_invalid(void) {
    TuningSystem *tuning_system = make_tuning_system();
    long adjustments[1] = {1};
    IntervallicManipulation *manipulation = make_manipulation("intervallic-invalid", adjustments, 1);

    PitchNumericValue *values[3];
    NoteEvent *events[3];
    for (int i = 0; i < 3; i++) {
        assert(pitch_numeric_value_create(tuning_system, 60 + i, 1, &values[i]) == PITCH_NUMERIC_VALUE_OK);
        events[i] = make_note_event("note", i);
    }
    const PitchNumericValue **pitches = (const PitchNumericValue **)values;

    TransformResult *result = NULL;
    assert(intervallic_manipulation_apply(NULL, events, pitches, 2, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_MANIPULATION);
    assert(intervallic_manipulation_apply(manipulation, NULL, pitches, 2, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_EVENTS);
    assert(intervallic_manipulation_apply(manipulation, events, NULL, 2, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_ERR_NULL_PITCH_VALUES);
    assert(intervallic_manipulation_apply(manipulation, events, pitches, 0, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_ERR_EMPTY);
    /* one adjustment needs exactly two events */
    assert(intervallic_manipulation_apply(manipulation, events, pitches, 3, &result) ==
           INTERVALLIC_MANIPULATION_APPLY_ERR_ADJUSTMENT_COUNT_MISMATCH);
    assert(result == NULL);

    for (int i = 0; i < 3; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(values[i]);
    }
    intervallic_manipulation_destroy(manipulation);
    tuning_system_destroy(tuning_system);
}

int main(void) {
    test_create();
    test_create_rejects_invalid();
    test_apply_widens_in_each_direction();
    test_apply_narrows_and_carries_on();
    test_apply_rejects_invalid();
    printf("test_intervallic_manipulation: ok\n");
    return 0;
}
