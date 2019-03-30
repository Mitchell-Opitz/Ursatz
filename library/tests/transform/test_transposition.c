#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/transposition.h"

/*
 * Construction test: a Transposition declares pitch displacement
 * (semitones) and spelling behavior now, and requires tuning
 * behavior/range policy/instrument context to be explicitly declared
 * absent (TDD §141) rather than silently omitted.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("transposition-up-major-third", &id) == ENTITY_ID_OK);

    Transposition *transposition;
    assert(transposition_create(id, 4, "diatonic, preserve letter-name interval", false, false, false,
                                 make_absent_provenance(), &transposition) == TRANSPOSITION_OK);

    assert(transposition_semitones(transposition) == 4);
    assert(strcmp(transposition_spelling_behavior(transposition), "diatonic, preserve letter-name interval") == 0);
    assert(!transposition_has_tuning_behavior(transposition));
    assert(!transposition_has_range_policy(transposition));
    assert(!transposition_has_instrument_context(transposition));

    const Transformation *base = transposition_transformation(transposition);
    assert(strcmp(transformation_input_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_output_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == transposition_id(transposition));

    EntityID *other_id;
    assert(entity_id_create("transposition-down-perfect-fifth", &other_id) == ENTITY_ID_OK);
    Transposition *other;
    assert(transposition_create(other_id, -7, "diatonic, preserve letter-name interval", false, false, false,
                                 make_absent_provenance(), &other) == TRANSPOSITION_OK);

    assert(transposition_identity_equals(transposition, transposition));
    assert(!transposition_identity_equals(transposition, other));

    EntityID *rejected_id;
    assert(entity_id_create("transposition-rejects-tuning", &rejected_id) == ENTITY_ID_OK);
    Transposition *rejected;
    assert(transposition_create(rejected_id, 1, "diatonic", true, false, false, make_absent_provenance(),
                                 &rejected) == TRANSPOSITION_ERR_TUNING_BEHAVIOR_NOT_SUPPORTED);
    entity_id_destroy(rejected_id);

    transposition_destroy(transposition);
    transposition_destroy(other);

    printf("test_transposition: ok\n");
    return 0;
}