#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "transform/retrograde_inversion.h"

/*
 * Construction test: RetrogradeInversion composes a Retrograde and an
 * Inversion rather than reimplementing either -- both are reachable
 * from the composed object and each carries the expected values from
 * this constructor's arguments.
 */

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static PitchIdentity *make_axis(const char *token) {
    PitchIdentity *axis;
    assert(pitch_identity_create(token, strlen(token), &axis) == PITCH_IDENTITY_OK);
    return axis;
}

int main(void) {
    EntityID *id;
    assert(entity_id_create("retrograde-inversion-k1", &id) == ENTITY_ID_OK);

    unsigned mask = RETROGRADE_DIMENSION_EVENT_ORDER;
    RetrogradeInversion *ri;
    assert(retrograde_inversion_create(id, mask, make_axis("pitch-middle-c"), "chromatic pitch space",
                                        "diatonic, preserve letter-name distance from axis", make_absent_provenance(),
                                        &ri) == RETROGRADE_INVERSION_OK);

    const Retrograde *retrograde = retrograde_inversion_retrograde(ri);
    assert(retrograde_reversed_dimensions(retrograde) == mask);

    const Inversion *inversion = retrograde_inversion_inversion(ri);
    assert(strcmp(inversion_pitch_space(inversion), "chromatic pitch space") == 0);
    assert(strcmp(inversion_spelling_policy(inversion), "diatonic, preserve letter-name distance from axis") == 0);

    const Transformation *base = retrograde_inversion_transformation(ri);
    assert(strcmp(transformation_input_type(base), "Structure") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == retrograde_inversion_id(ri));

    /* The component ids are distinct from the composed id and from each other. */
    assert(!entity_id_equals(retrograde_id(retrograde), retrograde_inversion_id(ri)));
    assert(!entity_id_equals(inversion_id(inversion), retrograde_inversion_id(ri)));
    assert(!entity_id_equals(retrograde_id(retrograde), inversion_id(inversion)));

    EntityID *other_id;
    assert(entity_id_create("retrograde-inversion-k2", &other_id) == ENTITY_ID_OK);
    RetrogradeInversion *other;
    assert(retrograde_inversion_create(other_id, mask, make_axis("pitch-a4"), "chromatic pitch space",
                                        "diatonic, preserve letter-name distance from axis", make_absent_provenance(),
                                        &other) == RETROGRADE_INVERSION_OK);

    assert(retrograde_inversion_identity_equals(ri, ri));
    assert(!retrograde_inversion_identity_equals(ri, other));

    EntityID *bad_id;
    assert(entity_id_create("retrograde-inversion-rejects-no-dimensions", &bad_id) == ENTITY_ID_OK);
    RetrogradeInversion *bad;
    assert(retrograde_inversion_create(bad_id, 0, make_axis("pitch-middle-c"), "chromatic pitch space", "diatonic",
                                        make_absent_provenance(),
                                        &bad) == RETROGRADE_INVERSION_ERR_NO_DIMENSIONS);
    entity_id_destroy(bad_id);

    retrograde_inversion_destroy(ri);
    retrograde_inversion_destroy(other);

    printf("test_retrograde_inversion: ok\n");
    return 0;
}