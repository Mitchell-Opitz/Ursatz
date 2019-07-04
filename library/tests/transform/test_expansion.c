#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/expansion.h"

/*
 * Construction test: Expansion extends a Structure with appended
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
    EntityID *id = make_id("expansion-extend-theme-a");
    EntityID *source_structure_id = make_id("structure-theme-a");
    EntityID *event_ids[2] = {make_id("note-event-5"), make_id("note-event-6")};

    Expansion *expansion;
    assert(expansion_create(id, source_structure_id, event_ids, 2, make_absent_provenance(), &expansion) ==
           EXPANSION_OK);

    assert(entity_id_equals(expansion_source_structure_id(expansion), source_structure_id));
    assert(expansion_appended_event_id_count(expansion) == 2);
    assert(strcmp(entity_id_value(expansion_appended_event_id_at(expansion, 0)), "note-event-5") == 0);
    assert(strcmp(entity_id_value(expansion_appended_event_id_at(expansion, 1)), "note-event-6") == 0);

    const Transformation *base = expansion_transformation(expansion);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "event membership (extension)") == 0);
    assert(transformation_id(base) == expansion_id(expansion));

    EntityID *other_id = make_id("expansion-other");
    EntityID *other_source_id = make_id("structure-theme-b");
    EntityID *other_event_ids[1] = {make_id("note-event-7")};
    Expansion *other;
    assert(expansion_create(other_id, other_source_id, other_event_ids, 1, make_absent_provenance(), &other) ==
           EXPANSION_OK);

    assert(expansion_identity_equals(expansion, expansion));
    assert(!expansion_identity_equals(expansion, other));

    EntityID *empty_id = make_id("expansion-rejects-empty");
    EntityID *empty_source_id = make_id("structure-theme-c");
    Expansion *empty;
    assert(expansion_create(empty_id, empty_source_id, NULL, 0, make_absent_provenance(), &empty) ==
           EXPANSION_ERR_NULL_APPENDED_EVENT_IDS);
    entity_id_destroy(empty_id);
    entity_id_destroy(empty_source_id);

    EntityID *null_entry_id = make_id("expansion-rejects-null-entry");
    EntityID *null_entry_source_id = make_id("structure-theme-d");
    EntityID *bad_event_ids[2] = {make_id("note-event-8"), NULL};
    Expansion *null_entry;
    assert(expansion_create(null_entry_id, null_entry_source_id, bad_event_ids, 2, make_absent_provenance(),
                             &null_entry) == EXPANSION_ERR_NULL_APPENDED_EVENT_ID);
    entity_id_destroy(null_entry_id);
    entity_id_destroy(null_entry_source_id);
    entity_id_destroy(bad_event_ids[0]);

    expansion_destroy(expansion);
    expansion_destroy(other);

    printf("test_expansion: ok\n");
    return 0;
}