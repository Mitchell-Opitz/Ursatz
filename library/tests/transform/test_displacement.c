#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "time/duration.h"
#include "transform/displacement.h"

/*
 * Construction test: Displacement shifts only a TemporalAnchor
 * (declared via direction + non-negative magnitude), leaving every
 * other event field untouched per the base Transformation's
 * preserved_dimensions.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration;
    assert(duration_create(numerator, denominator, &duration) == DURATION_OK);
    return duration;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("displacement-delay-eighth-note", &id) == ENTITY_ID_OK);

    Displacement *displacement;
    assert(displacement_create(id, DISPLACEMENT_DIRECTION_FORWARD, make_duration(1, 8), make_absent_provenance(),
                                &displacement) == DISPLACEMENT_OK);

    assert(displacement_direction(displacement) == DISPLACEMENT_DIRECTION_FORWARD);
    assert(duration_numerator(displacement_magnitude(displacement)) == 1);
    assert(duration_denominator(displacement_magnitude(displacement)) == 8);

    const Transformation *base = displacement_transformation(displacement);
    assert(strcmp(transformation_modified_dimensions(base), "temporal position (TemporalAnchor)") == 0);
    assert(strcmp(transformation_preserved_dimensions(base), "pitch, duration, articulation") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == displacement_id(displacement));

    EntityID *other_id;
    assert(entity_id_create("displacement-anticipate-sixteenth-note", &other_id) == ENTITY_ID_OK);
    Displacement *other;
    assert(displacement_create(other_id, DISPLACEMENT_DIRECTION_BACKWARD, make_duration(1, 16),
                                make_absent_provenance(), &other) == DISPLACEMENT_OK);
    assert(displacement_direction(other) == DISPLACEMENT_DIRECTION_BACKWARD);

    assert(displacement_identity_equals(displacement, displacement));
    assert(!displacement_identity_equals(displacement, other));

    EntityID *bad_id;
    assert(entity_id_create("displacement-rejects-null-magnitude", &bad_id) == ENTITY_ID_OK);
    Displacement *bad;
    assert(displacement_create(bad_id, DISPLACEMENT_DIRECTION_FORWARD, NULL, make_absent_provenance(), &bad) ==
           DISPLACEMENT_ERR_NULL_MAGNITUDE);
    entity_id_destroy(bad_id);

    displacement_destroy(displacement);
    displacement_destroy(other);

    printf("test_displacement: ok\n");
    return 0;
}