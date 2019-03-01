#include <assert.h>
#include <stdio.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "relationship/relationship.h"

/*
 * Relationship-category test (Test Spec Ref = "Relationship"): confirms
 * construction succeeds with the required fields, and confirms
 * source/target are references (EntityID identity), not embedded
 * copies -- relationship_source_id/relationship_target_id return
 * exactly the EntityID pointers passed in, and Relationship exposes no
 * accessor that returns a resolved source/target object.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *id = make_id("relationship-1");
    EntityID *source_id = make_id("motif-1");
    EntityID *target_id = make_id("motif-2");

    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Relationship *relationship;
    RelationshipResult result = relationship_create(id, source_id, "derived_from", target_id, NULL, NULL, NULL,
                                                      NULL, NULL, provenance, &relationship);
    assert(result == RELATIONSHIP_OK);

    /* source_id/target_id come back as exactly the references passed in. */
    assert(relationship_source_id(relationship) == source_id);
    assert(relationship_target_id(relationship) == target_id);
    assert(entity_id_equals(relationship_source_id(relationship), source_id));
    assert(entity_id_equals(relationship_target_id(relationship), target_id));

    /* Optional reference fields left unset stay NULL, not defaulted. */
    assert(relationship_context_id(relationship) == NULL);
    assert(relationship_evidence_id(relationship) == NULL);
    assert(relationship_scope(relationship) == NULL);
    assert(relationship_qualifiers(relationship) == NULL);
    assert(relationship_uncertainty(relationship) == NULL);

    assert(relationship_provenance(relationship) != NULL);
    assert(provenance_reference_state(relationship_provenance(relationship)) == PROVENANCE_REFERENCE_ABSENT);

    relationship_destroy(relationship);

    /* Missing required fields are rejected. */
    EntityID *id_2 = make_id("relationship-2");
    EntityID *source_2 = make_id("motif-3");
    EntityID *target_2 = make_id("motif-4");
    ProvenanceReference *provenance_2;
    assert(provenance_reference_create_absent(&provenance_2) == PROVENANCE_REFERENCE_OK);

    Relationship *rejected;
    assert(relationship_create(NULL, source_2, "derived_from", target_2, NULL, NULL, NULL, NULL, NULL,
                                provenance_2, &rejected) == RELATIONSHIP_ERR_NULL_ID);

    entity_id_destroy(id_2);
    entity_id_destroy(source_2);
    entity_id_destroy(target_2);
    provenance_reference_destroy(provenance_2);

    printf("test_relationship: ok\n");
    return 0;
}