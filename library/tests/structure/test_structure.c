#include <assert.h>
#include <stdio.h>

#include "core/entity.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "structure/structure.h"

/*
 * Invariant test: a Structure holds its referenced events as EntityID
 * pointers only, never a copy of NoteEvent data (TDD Sec56, Sec152).
 * structure_create's signature only accepts EntityID*, so this test
 * confirms the accessor returns exactly the id references passed in
 * (by identity equality), and nothing more is retrievable through the
 * Structure than those references and the owning Entity.
 */

static Entity *make_entity(const char *id_value) {
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    SchemaVersion *schema_version;
    assert(schema_version_create(1, &schema_version) == SCHEMA_VERSION_OK);

    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Entity *entity;
    assert(entity_create(id, "structure", schema_version, NULL, provenance, &entity) == ENTITY_OK);
    return entity;
}

int main(void) {
    Entity *entity = make_entity("structure-1");

    EntityID *event_id_a;
    assert(entity_id_create("event-a", &event_id_a) == ENTITY_ID_OK);
    EntityID *event_id_b;
    assert(entity_id_create("event-b", &event_id_b) == ENTITY_ID_OK);
    EntityID *event_ids[2] = {event_id_a, event_id_b};

    Structure *structure;
    StructureResult result = structure_create(entity, event_ids, 2, &structure);
    assert(result == STRUCTURE_OK);

    assert(structure_event_id_count(structure) == 2);

    EntityID *expected_a;
    assert(entity_id_create("event-a", &expected_a) == ENTITY_ID_OK);
    EntityID *expected_b;
    assert(entity_id_create("event-b", &expected_b) == ENTITY_ID_OK);

    assert(entity_id_equals(structure_event_id_at(structure, 0), expected_a));
    assert(entity_id_equals(structure_event_id_at(structure, 1), expected_b));

    entity_id_destroy(expected_a);
    entity_id_destroy(expected_b);

    structure_destroy(structure);

    printf("test_structure: ok\n");
    return 0;
}