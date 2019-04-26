#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/ornamentation.h"

/*
 * Construction test: Ornamentation requires an explicit one-to-many
 * provenance mapping -- every derived event's ProvenanceReference must
 * be PRESENT and target the source event, not merely implied by array
 * position.
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

    ornamentation_destroy(ornamentation);

    printf("test_ornamentation: ok\n");
    return 0;
}