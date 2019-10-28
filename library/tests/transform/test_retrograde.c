#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/retrograde.h"

/*
 * Construction test: Retrograde requires an explicit, non-zero
 * dimension mask (TDD §143) -- no default "reverse everything"
 * behavior, and no dimension bit outside the declared vocabulary.
 *
 * Apply test: retrograde_apply reflects each event's onset about the
 * sequence's own span, reversing temporal order while leaving pitch
 * and duration untouched. Requesting any dimension besides
 * RETROGRADE_DIMENSION_EVENT_ORDER fails with
 * RETROGRADE_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static NoteEvent *make_note_event(const char *id_value, long onset_numerator, unsigned long onset_denominator,
                                   long duration_numerator_value, unsigned long duration_denominator_value) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    MusicalTime *position;
    assert(musical_time_create(onset_numerator, onset_denominator, &position) == MUSICAL_TIME_OK);
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
    assert(entity_id_create("retrograde-pitch-and-rhythm", &id) == ENTITY_ID_OK);

    unsigned mask = RETROGRADE_DIMENSION_PITCH | RETROGRADE_DIMENSION_RHYTHM;
    Retrograde *retrograde;
    assert(retrograde_create(id, mask, make_absent_provenance(), &retrograde) == RETROGRADE_OK);

    assert(retrograde_reversed_dimensions(retrograde) == mask);

    const Transformation *base = retrograde_transformation(retrograde);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch, rhythm") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "event order, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == retrograde_id(retrograde));

    EntityID *all_id;
    assert(entity_id_create("retrograde-all", &all_id) == ENTITY_ID_OK);
    Retrograde *all;
    assert(retrograde_create(all_id, RETROGRADE_DIMENSION_ALL, make_absent_provenance(), &all) == RETROGRADE_OK);
    const Transformation *all_base = retrograde_transformation(all);
    assert(strcmp(transformation_modified_dimensions(all_base), "event order, pitch, rhythm, articulation") == 0);
    assert(strcmp(transformation_preserved_dimensions(all_base), "none (all reversed)") == 0);

    assert(retrograde_identity_equals(retrograde, retrograde));
    assert(!retrograde_identity_equals(retrograde, all));

    EntityID *zero_id;
    assert(entity_id_create("retrograde-rejects-no-dimensions", &zero_id) == ENTITY_ID_OK);
    Retrograde *zero;
    assert(retrograde_create(zero_id, 0, make_absent_provenance(), &zero) == RETROGRADE_ERR_NO_DIMENSIONS);
    entity_id_destroy(zero_id);

    EntityID *bad_id;
    assert(entity_id_create("retrograde-rejects-unknown-bits", &bad_id) == ENTITY_ID_OK);
    Retrograde *bad;
    assert(retrograde_create(bad_id, 1u << 30, make_absent_provenance(), &bad) ==
           RETROGRADE_ERR_UNKNOWN_DIMENSION_BITS);
    entity_id_destroy(bad_id);

    /* Apply: two events, onsets 0/4 and 1/4, durations both 1/4 -- span
       is [0/4, 2/4]. Reflecting: event at 0/4 (ends 1/4) -> new onset
       2/4 - 1/4 = 1/4; event at 1/4 (ends 2/4) -> new onset 2/4 - 2/4 =
       0/4. Order is reversed; durations/pitch are untouched. */
    EntityID *order_id;
    assert(entity_id_create("retrograde-order-only", &order_id) == ENTITY_ID_OK);
    Retrograde *order_only;
    assert(retrograde_create(order_id, RETROGRADE_DIMENSION_EVENT_ORDER, make_absent_provenance(), &order_only) ==
           RETROGRADE_OK);

    NoteEvent *first = make_note_event("note-first", 0, 4, 1, 4);
    NoteEvent *second = make_note_event("note-second", 1, 4, 1, 4);
    NoteEvent *events[2];
    events[0] = first;
    events[1] = second;

    TransformResult *result;
    assert(retrograde_apply(order_only, events, 2, &result) == RETROGRADE_APPLY_OK);
    assert(transform_result_event_count(result) == 2);

    const NoteEvent *derived_first = transform_result_event_at(result, 0);
    const NoteEvent *derived_second = transform_result_event_at(result, 1);
    assert(musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(derived_first))) == 1);
    assert(musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(derived_first))) == 4);
    /* new_onset for the second event reduces to 0/1 (0/4 in lowest terms). */
    assert(musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(derived_second))) == 0);
    assert(musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(derived_second))) == 1);
    assert(duration_numerator(note_event_duration(derived_first)) == 1);
    assert(duration_denominator(note_event_duration(derived_first)) == 4);
    assert(pitch_identity_equals(note_event_pitch(derived_first), note_event_pitch(first)));

    const ProvenanceReference *first_provenance = note_event_provenance(derived_first);
    assert(provenance_reference_state(first_provenance) == PROVENANCE_REFERENCE_PRESENT);
    EntityID *expected_source = NULL;
    assert(entity_id_create("note-first", &expected_source) == ENTITY_ID_OK);
    assert(entity_id_equals(provenance_reference_target_id(first_provenance), expected_source));
    entity_id_destroy(expected_source);

    /* NULL/empty inputs are rejected -- checked before order_only is destroyed. */
    TransformResult *rejected_result = NULL;
    assert(retrograde_apply(NULL, events, 2, &rejected_result) == RETROGRADE_APPLY_ERR_NULL_RETROGRADE);
    assert(rejected_result == NULL);
    assert(retrograde_apply(order_only, NULL, 2, &rejected_result) == RETROGRADE_APPLY_ERR_NULL_EVENTS);
    assert(rejected_result == NULL);
    assert(retrograde_apply(order_only, events, 0, &rejected_result) == RETROGRADE_APPLY_ERR_EMPTY);
    assert(rejected_result == NULL);

    transform_result_destroy(result);
    note_event_destroy(first);
    note_event_destroy(second);
    retrograde_destroy(order_only);

    /* Requesting a not-yet-implemented dimension in the same apply call fails cleanly. */
    NoteEvent *lone = make_note_event("note-lone", 0, 4, 1, 4);
    NoteEvent *lone_events[1];
    lone_events[0] = lone;
    TransformResult *not_implemented_result = NULL;
    assert(retrograde_apply(retrograde, lone_events, 1, &not_implemented_result) ==
           RETROGRADE_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED);
    assert(not_implemented_result == NULL);
    note_event_destroy(lone);

    retrograde_destroy(retrograde);
    retrograde_destroy(all);

    printf("test_retrograde: ok\n");
    return 0;
}