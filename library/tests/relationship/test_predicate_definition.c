#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "relationship/predicate_definition.h"
#include "relationship/relationship.h"

/*
 * Structural (relationship) test: confirms a PredicateDefinition can
 * be constructed with the TDD §76 fields, and confirms it can be
 * referenced by a Relationship -- a Relationship's predicate name
 * (relationship_predicate_name) matches the PredicateDefinition's own
 * name, the correlation this module relies on since Relationship
 * holds no compile-time/object reference to PredicateDefinition.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

int main(void) {
    EntityID *definition_id = make_id("predicate-derived-from");
    const char *source_types[] = {"Motif"};
    const char *target_types[] = {"Motif"};

    PredicateDefinition *definition;
    PredicateDefinitionResult result =
        predicate_definition_create(definition_id, "derived_from", NULL, false, true, source_types, 1,
                                     target_types, 1, false, &definition);
    assert(result == PREDICATE_DEFINITION_OK);

    assert(strcmp(predicate_definition_name(definition), "derived_from") == 0);
    assert(predicate_definition_inverse(definition) == NULL);
    assert(predicate_definition_symmetric(definition) == false);
    assert(predicate_definition_transitive(definition) == true);
    assert(predicate_definition_contextual(definition) == false);

    assert(predicate_definition_allowed_source_type_count(definition) == 1);
    assert(strcmp(predicate_definition_allowed_source_type_at(definition, 0), "Motif") == 0);
    assert(predicate_definition_allowed_target_type_count(definition) == 1);
    assert(strcmp(predicate_definition_allowed_target_type_at(definition, 0), "Motif") == 0);

    /* Referenced by a Relationship: correlation is by name string. */
    EntityID *relationship_id = make_id("relationship-1");
    EntityID *source_id = make_id("motif-1");
    EntityID *target_id = make_id("motif-2");
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Relationship *relationship;
    assert(relationship_create(relationship_id, source_id, "derived_from", target_id, NULL, NULL, NULL, NULL,
                                NULL, provenance, &relationship) == RELATIONSHIP_OK);

    assert(strcmp(relationship_predicate_name(relationship), predicate_definition_name(definition)) == 0);

    relationship_destroy(relationship);
    predicate_definition_destroy(definition);

    /* Symmetric with a known inverse. */
    EntityID *resembles_id = make_id("predicate-resembles");
    PredicateDefinition *resembles;
    assert(predicate_definition_create(resembles_id, "resembles", "resembles", true, false, NULL, 0, NULL, 0,
                                        true, &resembles) == PREDICATE_DEFINITION_OK);
    assert(strcmp(predicate_definition_inverse(resembles), "resembles") == 0);
    assert(predicate_definition_symmetric(resembles) == true);
    assert(predicate_definition_allowed_source_type_count(resembles) == 0);
    predicate_definition_destroy(resembles);

    printf("test_predicate_definition: ok\n");
    return 0;
}