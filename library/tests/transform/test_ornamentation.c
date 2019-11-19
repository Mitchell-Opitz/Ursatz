#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/ornamentation.h"

/*
 * Construction test: Ornamentation requires an explicit one-to-many
 * provenance mapping -- every derived event's ProvenanceReference must
 * be PRESENT and target the source event, not merely implied by array
 * position.
 *
 * Apply test: ornamentation_apply builds real derived NoteEvents from
 * caller-supplied OrnamentEventSpec values (offset/duration/pitch) --
 * the caller-supplied "derivation rule" this pass scopes Ornamentation
 * to, no hardcoded ornament vocabulary. Each derived event's id and
 * ProvenanceReference come from what ornamentation_create already
 * declared and validated, not newly minted.
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

static ProvenanceReference *make_present_provenance(const char *target_value) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_present(make_id(target_value), &provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static NoteEvent *make_note_event(const char *id_value, long onset_numerator, unsigned long onset_denominator) {
    EntityID *id = make_id(id_value);
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
    EntityID *id = make_id("ornamentation-trill-on-d5");
    EntityID *source_event_id = make_id("note-event-source");
    EntityID *derived_ids[3] = {make_id("note-event-trill-1"), make_id("note-event-trill-2"),
                                 make_id("note-event-trill-3")};
    ProvenanceReference *derived_provenances[3] = {
        make_present_provenance("note-event-source"), make_present_provenance("note-event-source"),
        make_present_provenance("note-event-source")};

    Ornamentation *ornamentation;
    assert(ornamentation_create(id, source_event_id, derived_ids, derived_provenances, 3, make_absent_provenance(),
                                 &ornamentation) == ORNAMENTATION_OK);

    assert(entity_id_equals(ornamentation_source_event_id(ornamentation), source_event_id));
    assert(ornamentation_derived_event_count(ornamentation) == 3);
    for (size_t i = 0; i < 3; i++) {
        const ProvenanceReference *p = ornamentation_derived_event_provenance_at(ornamentation, i);
        assert(provenance_reference_state(p) == PROVENANCE_REFERENCE_PRESENT);
        assert(entity_id_equals(provenance_reference_target_id(p), source_event_id));
    }
    assert(strcmp(entity_id_value(ornamentation_derived_event_id_at(ornamentation, 1)), "note-event-trill-2") == 0);

    const Transformation *base = ornamentation_transformation(ornamentation);
    assert(strcmp(transformation_mapping_behavior(base),
                  "one-to-many: the source event derives multiple decorative events, each carrying an explicit "
                  "ProvenanceReference back to the source") == 0);
    assert(transformation_id(base) == ornamentation_id(ornamentation));

    assert(ornamentation_identity_equals(ornamentation, ornamentation));

    /* Rejects an absent provenance entry. */
    EntityID *absent_id = make_id("ornamentation-rejects-absent-provenance");
    EntityID *absent_source_id = make_id("note-event-source-2");
    EntityID *absent_derived_ids[1] = {make_id("note-event-derived-2")};
    ProvenanceReference *absent_provenances[1] = {make_absent_provenance()};
    Ornamentation *absent_case;
    assert(ornamentation_create(absent_id, absent_source_id, absent_derived_ids, absent_provenances, 1,
                                 make_absent_provenance(),
                                 &absent_case) == ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_ABSENT);
    entity_id_destroy(absent_id);
    entity_id_destroy(absent_source_id);
    entity_id_destroy(absent_derived_ids[0]);
    provenance_reference_destroy(absent_provenances[0]);

    /* Rejects a provenance entry targeting the wrong source. */
    EntityID *mismatch_id = make_id("ornamentation-rejects-mismatched-provenance");
    EntityID *mismatch_source_id = make_id("note-event-source-3");
    EntityID *mismatch_derived_ids[1] = {make_id("note-event-derived-3")};
    ProvenanceReference *mismatch_provenances[1] = {make_present_provenance("note-event-wrong-source")};
    Ornamentation *mismatch_case;
    assert(ornamentation_create(mismatch_id, mismatch_source_id, mismatch_derived_ids, mismatch_provenances, 1,
                                 make_absent_provenance(),
                                 &mismatch_case) == ORNAMENTATION_ERR_DERIVED_EVENT_PROVENANCE_TARGET_MISMATCH);
    entity_id_destroy(mismatch_id);
    entity_id_destroy(mismatch_source_id);
    entity_id_destroy(mismatch_derived_ids[0]);
    provenance_reference_destroy(mismatch_provenances[0]);

    /* Apply: source event onset 1/4, derived events at offsets
       0/1, 1/8, 1/4 -- matching this fixture's declared derived ids
       (note-event-trill-1/2/3) and provenances (both already
       validated by ornamentation_create above). */
    NoteEvent *source_event = make_note_event("note-event-source", 1, 4);

    Duration *offset0;
    assert(duration_create(0, 1, &offset0) == DURATION_OK);
    Duration *offset1;
    assert(duration_create(1, 8, &offset1) == DURATION_OK);
    Duration *offset2;
    assert(duration_create(1, 4, &offset2) == DURATION_OK);

    Duration *spec_duration0;
    assert(duration_create(1, 8, &spec_duration0) == DURATION_OK);
    Duration *spec_duration1;
    assert(duration_create(1, 8, &spec_duration1) == DURATION_OK);
    Duration *spec_duration2;
    assert(duration_create(1, 4, &spec_duration2) == DURATION_OK);

    PitchIdentity *pitch0;
    assert(pitch_identity_create("d5", strlen("d5") + 1, &pitch0) == PITCH_IDENTITY_OK);
    PitchIdentity *pitch1;
    assert(pitch_identity_create("e5", strlen("e5") + 1, &pitch1) == PITCH_IDENTITY_OK);
    PitchIdentity *pitch2;
    assert(pitch_identity_create("d5", strlen("d5") + 1, &pitch2) == PITCH_IDENTITY_OK);

    OrnamentEventSpec specs[3] = {
        {offset0, spec_duration0, pitch0},
        {offset1, spec_duration1, pitch1},
        {offset2, spec_duration2, pitch2},
    };

    TransformResult *apply_result;
    assert(ornamentation_apply(ornamentation, source_event, specs, 3, &apply_result) == ORNAMENTATION_APPLY_OK);
    assert(transform_result_event_count(apply_result) == 3);

    const char *expected_derived_ids[3] = {"note-event-trill-1", "note-event-trill-2", "note-event-trill-3"};
    for (size_t i = 0; i < 3; i++) {
        const NoteEvent *derived = transform_result_event_at(apply_result, i);
        assert(strcmp(entity_id_value(note_event_id(derived)), expected_derived_ids[i]) == 0);

        const ProvenanceReference *derived_provenance = note_event_provenance(derived);
        assert(provenance_reference_state(derived_provenance) == PROVENANCE_REFERENCE_PRESENT);
        assert(entity_id_equals(provenance_reference_target_id(derived_provenance), source_event_id));
    }

    const MusicalTime *onset0 =
        temporal_anchor_position(note_event_temporal_anchor(transform_result_event_at(apply_result, 0)));
    assert(musical_time_numerator(onset0) == 1 && musical_time_denominator(onset0) == 4);
    const MusicalTime *onset1 =
        temporal_anchor_position(note_event_temporal_anchor(transform_result_event_at(apply_result, 1)));
    assert(musical_time_numerator(onset1) == 3 && musical_time_denominator(onset1) == 8);
    const MusicalTime *onset2 =
        temporal_anchor_position(note_event_temporal_anchor(transform_result_event_at(apply_result, 2)));
    assert(musical_time_numerator(onset2) == 1 && musical_time_denominator(onset2) == 2);

    transform_result_destroy(apply_result);

    /* Error paths. */
    TransformResult *rejected = NULL;
    assert(ornamentation_apply(NULL, source_event, specs, 3, &rejected) == ORNAMENTATION_APPLY_ERR_NULL_ORNAMENTATION);
    assert(ornamentation_apply(ornamentation, NULL, specs, 3, &rejected) == ORNAMENTATION_APPLY_ERR_NULL_SOURCE_EVENT);
    assert(ornamentation_apply(ornamentation, source_event, NULL, 3, &rejected) == ORNAMENTATION_APPLY_ERR_NULL_SPECS);
    assert(ornamentation_apply(ornamentation, source_event, specs, 2, &rejected) ==
           ORNAMENTATION_APPLY_ERR_SPEC_COUNT_MISMATCH);
    assert(rejected == NULL);

    OrnamentEventSpec bad_specs[3] = {
        {NULL, spec_duration0, pitch0},
        {offset1, spec_duration1, pitch1},
        {offset2, spec_duration2, pitch2},
    };
    assert(ornamentation_apply(ornamentation, source_event, bad_specs, 3, &rejected) ==
           ORNAMENTATION_APPLY_ERR_NULL_SPEC_FIELD);
    assert(rejected == NULL);

    note_event_destroy(source_event);
    duration_destroy(offset0);
    duration_destroy(offset1);
    duration_destroy(offset2);
    duration_destroy(spec_duration0);
    duration_destroy(spec_duration1);
    duration_destroy(spec_duration2);
    pitch_identity_destroy(pitch0);
    pitch_identity_destroy(pitch1);
    pitch_identity_destroy(pitch2);

    ornamentation_destroy(ornamentation);

    printf("test_ornamentation: ok\n");
    return 0;
}