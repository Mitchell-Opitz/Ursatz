#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/contraction.h"

/*
 * Construction test: Contraction compresses a Structure by dropping
 * events by reference (owned EntityID pointers) rather than by
 * copying NoteEvent payloads.
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

int main(void) {
    EntityID *id = make_id("contraction-trim-theme-a");
    EntityID *source_structure_id = make_id("structure-theme-a");
    EntityID *event_ids[2] = {make_id("note-event-9"), make_id("note-event-10")};

    Contraction *contraction;
    assert(contraction_create(id, source_structure_id, event_ids, 2, make_absent_provenance(), &contraction) ==
           CONTRACTION_OK);

    assert(entity_id_equals(contraction_source_structure_id(contraction), source_structure_id));
    assert(contraction_removed_event_id_count(contraction) == 2);
    assert(strcmp(entity_id_value(contraction_removed_event_id_at(contraction, 0)), "note-event-9") == 0);
    assert(strcmp(entity_id_value(contraction_removed_event_id_at(contraction, 1)), "note-event-10") == 0);

    const Transformation *base = contraction_transformation(contraction);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "event membership (compression)") == 0);
    assert(transformation_id(base) == contraction_id(contraction));

    EntityID *other_id = make_id("contraction-other");
    EntityID *other_source_id = make_id("structure-theme-b");
    EntityID *other_event_ids[1] = {make_id("note-event-11")};
    Contraction *other;
    assert(contraction_create(other_id, other_source_id, other_event_ids, 1, make_absent_provenance(), &other) ==
           CONTRACTION_OK);

    assert(contraction_identity_equals(contraction, contraction));
    assert(!contraction_identity_equals(contraction, other));

    EntityID *empty_id = make_id("contraction-rejects-empty");
    EntityID *empty_source_id = make_id("structure-theme-c");
    Contraction *empty;
    assert(contraction_create(empty_id, empty_source_id, NULL, 0, make_absent_provenance(), &empty) ==
           CONTRACTION_ERR_NULL_REMOVED_EVENT_IDS);
    entity_id_destroy(empty_id);
    entity_id_destroy(empty_source_id);

    EntityID *null_entry_id = make_id("contraction-rejects-null-entry");
    EntityID *null_entry_source_id = make_id("structure-theme-d");
    EntityID *bad_event_ids[2] = {make_id("note-event-12"), NULL};
    Contraction *null_entry;
    assert(contraction_create(null_entry_id, null_entry_source_id, bad_event_ids, 2, make_absent_provenance(),
                               &null_entry) == CONTRACTION_ERR_NULL_REMOVED_EVENT_ID);
    entity_id_destroy(null_entry_id);
    entity_id_destroy(null_entry_source_id);
    entity_id_destroy(bad_event_ids[0]);

    contraction_destroy(contraction);
    contraction_destroy(other);

    printf("test_contraction: ok\n");
    return 0;
}