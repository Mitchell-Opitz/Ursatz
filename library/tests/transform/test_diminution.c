#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/diminution.h"

/*
 * Construction test: Diminution requires an explicit dimension
 * (TDD §144) and a strictly-shrinking rational factor. Performance
 * timing is only touched when explicitly requested -- it is never
 * reachable via a default/zero-initialized dimension value.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("diminution-durations-half", &id) == ENTITY_ID_OK);

    Diminution *diminution;
    assert(diminution_create(id, AUGMENTATION_DIMENSION_DURATIONS, 1, 2, make_absent_provenance(), &diminution) ==
           DIMINUTION_OK);

    assert(diminution_dimension(diminution) == AUGMENTATION_DIMENSION_DURATIONS);
    assert(diminution_scale_numerator(diminution) == 1);
    assert(diminution_scale_denominator(diminution) == 2);

    const Transformation *base = diminution_transformation(diminution);
    assert(strcmp(transformation_modified_dimensions(base), "durations") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "pitch, event order, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == diminution_id(diminution));

    EntityID *performance_id;
    assert(entity_id_create("diminution-performance-timing", &performance_id) == ENTITY_ID_OK);
    Diminution *performance;
    assert(diminution_create(performance_id, AUGMENTATION_DIMENSION_PERFORMANCE_TIMING, 2, 3,
                              make_absent_provenance(), &performance) == DIMINUTION_OK);
    assert(strcmp(transformation_modified_dimensions(diminution_transformation(performance)), "performance timing") ==
           0);

    assert(diminution_identity_equals(diminution, diminution));
    assert(!diminution_identity_equals(diminution, performance));

    EntityID *stretch_id;
    assert(entity_id_create("diminution-rejects-stretching-factor", &stretch_id) == ENTITY_ID_OK);
    Diminution *stretch;
    assert(diminution_create(stretch_id, AUGMENTATION_DIMENSION_DURATIONS, 2, 1, make_absent_provenance(),
                              &stretch) == DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE);
    entity_id_destroy(stretch_id);

    EntityID *equal_id;
    assert(entity_id_create("diminution-rejects-unity-factor", &equal_id) == ENTITY_ID_OK);
    Diminution *equal;
    assert(diminution_create(equal_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 1, make_absent_provenance(), &equal) ==
           DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE);
    entity_id_destroy(equal_id);

    EntityID *zero_numerator_id;
    assert(entity_id_create("diminution-rejects-zero-numerator", &zero_numerator_id) == ENTITY_ID_OK);
    Diminution *zero_numerator;
    assert(diminution_create(zero_numerator_id, AUGMENTATION_DIMENSION_DURATIONS, 0, 1, make_absent_provenance(),
                              &zero_numerator) == DIMINUTION_ERR_NON_POSITIVE_DENOMINATOR);
    entity_id_destroy(zero_numerator_id);

    diminution_destroy(diminution);
    diminution_destroy(performance);

    printf("test_diminution: ok\n");
    return 0;
}