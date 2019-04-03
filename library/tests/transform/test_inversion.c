#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "transform/inversion.h"

/*
 * Construction test: an Inversion declares an explicit axis pitch
 * value, pitch space, and spelling policy now, and requires tuning
 * semantics to be explicitly declared absent (TDD §142) rather than
 * silently omitted or implicitly derived from input material.
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
    assert(entity_id_create("inversion-about-middle-c", &id) == ENTITY_ID_OK);

    PitchIdentity *axis = make_axis("pitch-middle-c");

    Inversion *inversion;
    assert(inversion_create(id, axis, "chromatic pitch space", "diatonic, preserve letter-name distance from axis",
                             false, make_absent_provenance(), &inversion) == INVERSION_OK);

    assert(pitch_identity_equals(inversion_axis(inversion), axis));
    assert(strcmp(inversion_pitch_space(inversion), "chromatic pitch space") == 0);
    assert(strcmp(inversion_spelling_policy(inversion), "diatonic, preserve letter-name distance from axis") == 0);
    assert(!inversion_has_tuning_semantics(inversion));

    const Transformation *base = inversion_transformation(inversion);
    assert(strcmp(transformation_input_type(base), "NoteEvent") == 0);
    assert(strcmp(transformation_modified_dimensions(base), "pitch") == 0);
    assert(transformation_deterministic(base));
    assert(transformation_id(base) == inversion_id(inversion));

    EntityID *other_id;
    assert(entity_id_create("inversion-about-a4", &other_id) == ENTITY_ID_OK);
    Inversion *other;
    assert(inversion_create(other_id, make_axis("pitch-a4"), "chromatic pitch space",
                             "diatonic, preserve letter-name distance from axis", false, make_absent_provenance(),
                             &other) == INVERSION_OK);

    assert(inversion_identity_equals(inversion, inversion));
    assert(!inversion_identity_equals(inversion, other));

    EntityID *rejected_id;
    assert(entity_id_create("inversion-rejects-tuning", &rejected_id) == ENTITY_ID_OK);
    Inversion *rejected;
    assert(inversion_create(rejected_id, make_axis("pitch-middle-c"), "chromatic pitch space", "diatonic", true,
                             make_absent_provenance(), &rejected) == INVERSION_ERR_TUNING_SEMANTICS_NOT_SUPPORTED);
    entity_id_destroy(rejected_id);

    inversion_destroy(inversion);
    inversion_destroy(other);

    printf("test_inversion: ok\n");
    return 0;
}