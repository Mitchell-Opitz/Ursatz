#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/diminution.h"

/*
 * Construction test: Diminution requires an explicit dimension
 * (TDD §144) and a strictly-shrinking rational factor. Performance
 * timing is only touched when explicitly requested -- it is never
 * reachable via a default/zero-initialized dimension value.
 *
 * Apply test: diminution_apply scales AUGMENTATION_DIMENSION_DURATIONS
 * by the exact rational factor; every other dimension fails with
 * DIMINUTION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED.
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
    assert(entity_id_create("diminution-durations-half", &id) == ENTITY_ID_OK);

    Diminution *diminution;
    assert(diminution_create(id, AUGMENTATION_DIMENSION_DURATIONS, 1, 2, make_absent_provenance(), &diminution) ==
           DIMINUTION_OK);

    assert(diminution_dimension(diminution) == AUGMENTATION_DIMENSION_DURATIONS);
    assert(diminution_scale_numerator(diminution) == 1);
    assert(diminution_scale_denominator(diminution) == 2);

    const Transformation *base = diminution_transformation(diminution);
    assert(strcmp(transformation_modified_dimensions(base), "durations") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "pitch, event order, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == diminution_id(diminution));

    EntityID *performance_id;
    assert(entity_id_create("diminution-performance-timing", &performance_id) == ENTITY_ID_OK);
    Diminution *performance;
    assert(diminution_create(performance_id, AUGMENTATION_DIMENSION_PERFORMANCE_TIMING, 2, 3,
                              make_absent_provenance(), &performance) == DIMINUTION_OK);
    assert(strcmp(transformation_modified_dimensions(diminution_transformation(performance)), "performance timing") ==
           0);

    assert(diminution_identity_equals(diminution, diminution));
    assert(!diminution_identity_equals(diminution, performance));

    EntityID *stretch_id;
    assert(entity_id_create("diminution-rejects-stretching-factor", &stretch_id) == ENTITY_ID_OK);
    Diminution *stretch;
    assert(diminution_create(stretch_id, AUGMENTATION_DIMENSION_DURATIONS, 2, 1, make_absent_provenance(),
                              &stretch) == DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE);
    entity_id_destroy(stretch_id);

    EntityID *equal_id;
    assert(entity_id_create("diminution-rejects-unity-factor", &equal_id) == ENTITY_ID_OK);
    Diminution *equal;
    assert(diminution_create(equal_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 1, make_absent_provenance(), &equal) ==
           DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE);
    entity_id_destroy(equal_id);

    EntityID *zero_numerator_id;
    assert(entity_id_create("diminution-rejects-zero-numerator", &zero_numerator_id) == ENTITY_ID_OK);
    Diminution *zero_numerator;
    assert(diminution_create(zero_numerator_id, AUGMENTATION_DIMENSION_DURATIONS, 0, 1, make_absent_provenance(),
                              &zero_numerator) == DIMINUTION_ERR_NON_POSITIVE_DENOMINATOR);
    entity_id_destroy(zero_numerator_id);

    /* Apply: a half note (1/2) halved by 1/2 -> quarter note (1/4). */
    NoteEvent *event = make_note_event("note-half", 1, 2);
    TransformResult *result;
    assert(diminution_apply(diminution, event, &result) == DIMINUTION_APPLY_OK);
    assert(transform_result_event_count(result) == 1);
    const NoteEvent *derived = transform_result_event_at(result, 0);
    assert(duration_numerator(note_event_duration(derived)) == 1);
    assert(duration_denominator(note_event_duration(derived)) == 4);
    assert(pitch_identity_equals(note_event_pitch(derived), note_event_pitch(event)));

    const ProvenanceReference *provenance = note_event_provenance(derived);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source;
    assert(entity_id_create("note-half", &expected_source) == ENTITY_ID_OK);
    assert(entity_id_equals(provenance_reference_target_id(provenance), expected_source));
    entity_id_destroy(expected_source);

    transform_result_destroy(result);

    /* A dimension other than DURATIONS has no real logic yet. */
    TransformResult *not_implemented_result = NULL;
    assert(diminution_apply(performance, event, &not_implemented_result) ==
           DIMINUTION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED);
    assert(not_implemented_result == NULL);

    TransformResult *rejected_result = NULL;
    assert(diminution_apply(NULL, event, &rejected_result) == DIMINUTION_APPLY_ERR_NULL_DIMINUTION);
    assert(diminution_apply(diminution, NULL, &rejected_result) == DIMINUTION_APPLY_ERR_NULL_EVENT);
    assert(rejected_result == NULL);

    note_event_destroy(event);
    diminution_destroy(diminution);
    diminution_destroy(performance);

    printf("test_diminution: ok\n");
    return 0;
}