#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/retrograde.h"

/*
 * Construction test: Retrograde requires an explicit, non-zero
 * dimension mask (TDD §143) -- no default "reverse everything"
 * behavior, and no dimension bit outside the declared vocabulary.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("retrograde-pitch-and-rhythm", &id) == ENTITY_ID_OK);

    unsigned mask = RETROGRADE_DIMENSION_PITCH | RETROGRADE_DIMENSION_RHYTHM;
    Retrograde *retrograde;
    assert(retrograde_create(id, mask, make_absent_provenance(), &retrograde) == RETROGRADE_OK);

    assert(retrograde_reversed_dimensions(retrograde) == mask);

    const Transformation *base = retrograde_transformation(retrograde);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch, rhythm") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "event order, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == retrograde_id(retrograde));

    EntityID *all_id;
    assert(entity_id_create("retrograde-all", &all_id) == ENTITY_ID_OK);
    Retrograde *all;
    assert(retrograde_create(all_id, RETROGRADE_DIMENSION_ALL, make_absent_provenance(), &all) == RETROGRADE_OK);
    const Transformation *all_base = retrograde_transformation(all);
    assert(strcmp(transformation_modified_dimensions(all_base), "event order, pitch, rhythm, articulation") == 0);
    assert(strcmp(transformation_preserved_dimensions(all_base), "none (all reversed)") == 0);

    assert(retrograde_identity_equals(retrograde, retrograde));
    assert(!retrograde_identity_equals(retrograde, all));

    EntityID *zero_id;
    assert(entity_id_create("retrograde-rejects-no-dimensions", &zero_id) == ENTITY_ID_OK);
    Retrograde *zero;
    assert(retrograde_create(zero_id, 0, make_absent_provenance(), &zero) == RETROGRADE_ERR_NO_DIMENSIONS);
    entity_id_destroy(zero_id);

    EntityID *bad_id;
    assert(entity_id_create("retrograde-rejects-unknown-bits", &bad_id) == ENTITY_ID_OK);
    Retrograde *bad;
    assert(retrograde_create(bad_id, 1u << 30, make_absent_provenance(), &bad) ==
           RETROGRADE_ERR_UNKNOWN_DIMENSION_BITS);
    entity_id_destroy(bad_id);

    retrograde_destroy(retrograde);
    retrograde_destroy(all);

    printf("test_retrograde: ok\n");
    return 0;
}