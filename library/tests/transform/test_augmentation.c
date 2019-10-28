#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/augmentation.h"

/*
 * Construction test: Augmentation requires an explicit dimension
 * (TDD §144) and a strictly-lengthening rational factor. Performance
 * timing is only touched when explicitly requested -- it is never
 * reachable via a default/zero-initialized dimension value.
 *
 * Apply test: augmentation_apply scales AUGMENTATION_DIMENSION_DURATIONS
 * by the exact rational factor; every other dimension fails with
 * AUGMENTATION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static NoteEvent *make_note_event(const char *id_value, long duration_numerator_value,
                                   unsigned long duration_denominator_value) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    MusicalTime *position;
    assert(musical_time_create(0, 4, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration;
    assert(duration_create(duration_numerator_value, duration_denominator_value, &duration) == DURATION_OK);
    PitchIdentity *pitch;
    assert(pitch_identity_create(id_value, strlen(id_value) + 1, &pitch) == PITCH_IDENTITY_OK);

    NoteEvent *note_event;
    assert(note_event_create(id, anchor, duration, pitch, NULL, NULL, make_absent_provenance(), &note_event) ==
           NOTE_EVENT_OK);
    return note_event;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("augmentation-durations-double", &id) == ENTITY_ID_OK);

    Augmentation *augmentation;
    assert(augmentation_create(id, AUGMENTATION_DIMENSION_DURATIONS, 2, 1, make_absent_provenance(),
                                &augmentation) == AUGMENTATION_OK);

    assert(augmentation_dimension(augmentation) == AUGMENTATION_DIMENSION_DURATIONS);
    assert(augmentation_scale_numerator(augmentation) == 2);
    assert(augmentation_scale_denominator(augmentation) == 1);

    const Transformation *base = augmentation_transformation(augmentation);
    assert(strcmp(transformation_modified_dimensions(base), "durations") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "pitch, event order, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == augmentation_id(augmentation));

    EntityID *performance_id;
    assert(entity_id_create("augmentation-performance-timing", &performance_id) == ENTITY_ID_OK);
    Augmentation *performance;
    assert(augmentation_create(performance_id, AUGMENTATION_DIMENSION_PERFORMANCE_TIMING, 3, 2,
                                make_absent_provenance(), &performance) == AUGMENTATION_OK);
    assert(strcmp(transformation_modified_dimensions(augmentation_transformation(performance)),
                  "performance timing") == 0);

    assert(augmentation_identity_equals(augmentation, augmentation));
    assert(!augmentation_identity_equals(augmentation, performance));

    EntityID *shrink_id;
    assert(entity_id_create("augmentation-rejects-shrinking-factor", &shrink_id) == ENTITY_ID_OK);
    Augmentation *shrink;
    assert(augmentation_create(shrink_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 2, make_absent_provenance(),
                                &shrink) == AUGMENTATION_ERR_FACTOR_NOT_GREATER_THAN_ONE);
    entity_id_destroy(shrink_id);

    EntityID *equal_id;
    assert(entity_id_create("augmentation-rejects-unity-factor", &equal_id) == ENTITY_ID_OK);
    Augmentation *equal;
    assert(augmentation_create(equal_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 1, make_absent_provenance(),
                                &equal) == AUGMENTATION_ERR_FACTOR_NOT_GREATER_THAN_ONE);
    entity_id_destroy(equal_id);

    EntityID *zero_denominator_id;
    assert(entity_id_create("augmentation-rejects-zero-denominator", &zero_denominator_id) == ENTITY_ID_OK);
    Augmentation *zero_denominator;
    assert(augmentation_create(zero_denominator_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 0, make_absent_provenance(),
                                &zero_denominator) == AUGMENTATION_ERR_NON_POSITIVE_DENOMINATOR);
    entity_id_destroy(zero_denominator_id);

    /* Apply: a quarter note (1/4) doubled by 2/1 -> half note (1/2). */
    NoteEvent *event = make_note_event("note-quarter", 1, 4);
    TransformResult *result;
    assert(augmentation_apply(augmentation, event, &result) == AUGMENTATION_APPLY_OK);
    assert(transform_result_event_count(result) == 1);
    const NoteEvent *derived = transform_result_event_at(result, 0);
    assert(duration_numerator(note_event_duration(derived)) == 1);
    assert(duration_denominator(note_event_duration(derived)) == 2);
    assert(pitch_identity_equals(note_event_pitch(derived), note_event_pitch(event)));

    const ProvenanceReference *provenance = note_event_provenance(derived);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source;
    assert(entity_id_create("note-quarter", &expected_source) == ENTITY_ID_OK);
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
    entity_id_destroy(expected_source);

    transform_result_destroy(result);

    /* A dimension other than DURATIONS has no real logic yet. */
    TransformResult *not_implemented_result = NULL;
    assert(augmentation_apply(performance, event, &not_implemented_result) ==
           AUGMENTATION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED);
    assert(not_implemented_result == NULL);

    TransformResult *rejected_result = NULL;
    assert(augmentation_apply(NULL, event, &rejected_result) == AUGMENTATION_APPLY_ERR_NULL_AUGMENTATION);
    assert(augmentation_apply(augmentation, NULL, &rejected_result) == AUGMENTATION_APPLY_ERR_NULL_EVENT);
    assert(rejected_result == NULL);

    note_event_destroy(event);
    augmentation_destroy(augmentation);
    augmentation_destroy(performance);

    printf("test_augmentation: ok\n");
    return 0;
}