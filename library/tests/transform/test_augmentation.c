#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "transform/augmentation.h"

/*
 * Construction test: Augmentation requires an explicit dimension
 * (TDD §144) and a strictly-lengthening rational factor. Performance
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
    assert(entity_id_create("augmentation-durations-double", &id) == ENTITY_ID_OK);

    Augmentation *augmentation;
    assert(augmentation_create(id, AUGMENTATION_DIMENSION_DURATIONS, 2, 1, make_absent_provenance(),
                                &augmentation) == AUGMENTATION_OK);

    assert(augmentation_dimension(augmentation) == AUGMENTATION_DIMENSION_DURATIONS);
    assert(augmentation_scale_numerator(augmentation) == 2);
    assert(augmentation_scale_denominator(augmentation) == 1);

    const Transformation *base = augmentation_transformation(augmentation);
    assert(strcmp(transformation_modified_dimensions(base), "durations") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "pitch, event order, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == augmentation_id(augmentation));

    EntityID *performance_id;
    assert(entity_id_create("augmentation-performance-timing", &performance_id) == ENTITY_ID_OK);
    Augmentation *performance;
    assert(augmentation_create(performance_id, AUGMENTATION_DIMENSION_PERFORMANCE_TIMING, 3, 2,
                                make_absent_provenance(), &performance) == AUGMENTATION_OK);
    assert(strcmp(transformation_modified_dimensions(augmentation_transformation(performance)),
                  "performance timing") == 0);

    assert(augmentation_identity_equals(augmentation, augmentation));
    assert(!augmentation_identity_equals(augmentation, performance));

    EntityID *shrink_id;
    assert(entity_id_create("augmentation-rejects-shrinking-factor", &shrink_id) == ENTITY_ID_OK);
    Augmentation *shrink;
    assert(augmentation_create(shrink_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 2, make_absent_provenance(),
                                &shrink) == AUGMENTATION_ERR_FACTOR_NOT_GREATER_THAN_ONE);
    entity_id_destroy(shrink_id);

    EntityID *equal_id;
    assert(entity_id_create("augmentation-rejects-unity-factor", &equal_id) == ENTITY_ID_OK);
    Augmentation *equal;
    assert(augmentation_create(equal_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 1, make_absent_provenance(),
                                &equal) == AUGMENTATION_ERR_FACTOR_NOT_GREATER_THAN_ONE);
    entity_id_destroy(equal_id);

    EntityID *zero_denominator_id;
    assert(entity_id_create("augmentation-rejects-zero-denominator", &zero_denominator_id) == ENTITY_ID_OK);
    Augmentation *zero_denominator;
    assert(augmentation_create(zero_denominator_id, AUGMENTATION_DIMENSION_DURATIONS, 1, 0, make_absent_provenance(),
                                &zero_denominator) == AUGMENTATION_ERR_NON_POSITIVE_DENOMINATOR);
    entity_id_destroy(zero_denominator_id);

    augmentation_destroy(augmentation);
    augmentation_destroy(performance);

    printf("test_augmentation: ok\n");
    return 0;
}