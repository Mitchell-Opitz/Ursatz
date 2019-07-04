#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/fragmentation.h"

/*
 * Construction test: Fragmentation extracts events by reference (owned
 * EntityID pointers) rather than by copying NoteEvent payloads --
 * there is no field here of NoteEvent-shaped content, only ids.
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
    EntityID *id = make_id("fragmentation-opening-motif");
    EntityID *source_structure_id = make_id("structure-theme-a");
    EntityID *event_ids[2] = {make_id("note-event-1"), make_id("note-event-2")};

    Fragmentation *fragmentation;
    assert(fragmentation_create(id, source_structure_id, event_ids, 2, make_absent_provenance(), &fragmentation) ==
           FRAGMENTATION_OK);

    assert(entity_id_equals(fragmentation_source_structure_id(fragmentation), source_structure_id));
    assert(fragmentation_extracted_event_id_count(fragmentation) == 2);
    assert(strcmp(entity_id_value(fragmentation_extracted_event_id_at(fragmentation, 0)), "note-event-1") == 0);
    assert(strcmp(entity_id_value(fragmentation_extracted_event_id_at(fragmentation, 1)), "note-event-2") == 0);

    const Transformation *base = fragmentation_transformation(fragmentation);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "event membership (subset extraction)") == 0);
    assert(transformation_id(base) == fragmentation_id(fragmentation));

    EntityID *other_id = make_id("fragmentation-other");
    EntityID *other_source_id = make_id("structure-theme-b");
    EntityID *other_event_ids[1] = {make_id("note-event-3")};
    Fragmentation *other;
    assert(fragmentation_create(other_id, other_source_id, other_event_ids, 1, make_absent_provenance(), &other) ==
           FRAGMENTATION_OK);

    assert(fragmentation_identity_equals(fragmentation, fragmentation));
    assert(!fragmentation_identity_equals(fragmentation, other));

    EntityID *empty_id = make_id("fragmentation-rejects-empty");
    EntityID *empty_source_id = make_id("structure-theme-c");
    Fragmentation *empty;
    assert(fragmentation_create(empty_id, empty_source_id, NULL, 0, make_absent_provenance(), &empty) ==
           FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_IDS);
    entity_id_destroy(empty_id);
    entity_id_destroy(empty_source_id);

    EntityID *null_entry_id = make_id("fragmentation-rejects-null-entry");
    EntityID *null_entry_source_id = make_id("structure-theme-d");
    EntityID *bad_event_ids[2] = {make_id("note-event-4"), NULL};
    Fragmentation *null_entry;
    assert(fragmentation_create(null_entry_id, null_entry_source_id, bad_event_ids, 2, make_absent_provenance(),
                                 &null_entry) == FRAGMENTATION_ERR_NULL_EXTRACTED_EVENT_ID);
    entity_id_destroy(null_entry_id);
    entity_id_destroy(null_entry_source_id);
    entity_id_destroy(bad_event_ids[0]);

    fragmentation_destroy(fragmentation);
    fragmentation_destroy(other);

    printf("test_fragmentation: ok\n");
    return 0;
}