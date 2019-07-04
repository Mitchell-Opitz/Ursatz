#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "transform/transformation.h"

/*
 * Construction test: a Transformation declares input type, parameters,
 * output type, preserved/modified dimensions, mapping behavior,
 * validation, determinism, and provenance (TDD §139).
 *
 * Purity test: transformation_create copies its required strings
 * rather than aliasing the caller's buffers -- mutating the caller's
 * buffer after construction does not change the Transformation's
 * accessors, confirming Transformation holds no live reference back
 * into caller-owned memory (TDD §140).
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("transformation-transposition-up-major-third", &id) == ENTITY_ID_OK);

    char output_type_buf[32];
    memcpy(output_type_buf, "NoteEvent", strlen("NoteEvent") + 1);

    Transformation *transformation;
    assert(transformation_create(id, "NoteEvent", NULL, output_type_buf, "rhythm, articulation", "pitch",
                                  "one-to-one, per-event pitch displacement", "pitch stays within MIDI range 0-127",
                                  true, make_absent_provenance(), &transformation) == TRANSFORMATION_OK);

    /* Mutate the caller's buffer; the Transformation must not see the change. */
    memcpy(output_type_buf, "Corrupted", strlen("Corrupted") + 1);

    assert(strcmp(transformation_input_type(transformation), "NoteEvent") == 0);
    assert(transformation_parameters(transformation) == NULL);
    assert(strcmp(transformation_output_type(transformation), "NoteEvent") == 0);
    assert(strcmp(transformation_preserved_dimensions(transformation), "rhythm, articulation") == 0);
    assert(strcmp(transformation_modified_dimensions(transformation), "pitch") == 0);
    assert(strcmp(transformation_mapping_behavior(transformation), "one-to-one, per-event pitch displacement") == 0);
    assert(strcmp(transformation_validation(transformation), "pitch stays within MIDI range 0-127") == 0);
    assert(transformation_deterministic(transformation));
    assert(provenance_reference_state(transformation_provenance(transformation)) == PROVENANCE_REFERENCE_ABSENT);

    EntityID *other_id;
    assert(entity_id_create("transformation-retrograde", &other_id) == ENTITY_ID_OK);
    Transformation *other;
    assert(transformation_create(other_id, "Structure", NULL, "Structure", "pitch, rhythm", "event order",
                                  "one-to-one, reversed event order", "event count unchanged", true,
                                  make_absent_provenance(), &other) == TRANSFORMATION_OK);

    assert(transformation_identity_equals(transformation, transformation));
    assert(!transformation_identity_equals(transformation, other));

    EntityID *missing_output_id;
    assert(entity_id_create("transformation-missing-output-type", &missing_output_id) == ENTITY_ID_OK);
    Transformation *invalid;
    assert(transformation_create(missing_output_id, "NoteEvent", NULL, "", "rhythm", "pitch", "one-to-one",
                                  "none", true, make_absent_provenance(),
                                  &invalid) == TRANSFORMATION_ERR_NULL_OUTPUT_TYPE);
    entity_id_destroy(missing_output_id);

    transformation_destroy(transformation);
    transformation_destroy(other);

    printf("test_transformation: ok\n");
    return 0;
}