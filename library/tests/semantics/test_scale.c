#include <assert.h>
#include <stdio.h>

#include "identity/entity_id.h"
#include "pitch/pitch_class.h"
#include "pitch/pitch_identity.h"
#include "semantics/scale.h"

/*
 * Golden test: a known TuningSystem reference and ordered PitchClass
 * set produce a known Scale shape -- every field comes back exactly as
 * given, nothing implicit added or dropped.
 */

static bool octave_equivalence(const PitchIdentity *a, const PitchIdentity *b) {
    return pitch_identity_equals(a, b);
}

static PitchClass *make_pitch_class(const char *token) {
    PitchIdentity *identity;
    assert(pitch_identity_create(token, 2, &identity) == PITCH_IDENTITY_OK);
    PitchClass *pitch_class;
    assert(pitch_class_create(identity, octave_equivalence, &pitch_class) == PITCH_CLASS_OK);
    return pitch_class;
}

int main(void) {
    EntityID *tuning_system_id;
    assert(entity_id_create("tuning-12tet", &tuning_system_id) == ENTITY_ID_OK);

    PitchClass *pitch_classes[3] = {make_pitch_class("c\0"), make_pitch_class("d\0"), make_pitch_class("e\0")};

    Scale *scale;
    assert(scale_create(tuning_system_id, pitch_classes, 3, &scale) == SCALE_OK);

    EntityID *expected_tuning_system_id;
    assert(entity_id_create("tuning-12tet", &expected_tuning_system_id) == ENTITY_ID_OK);
    assert(entity_id_equals(scale_tuning_system_id(scale), expected_tuning_system_id));
    entity_id_destroy(expected_tuning_system_id);

    assert(scale_pitch_class_count(scale) == 3);
    assert(scale_pitch_class_at(scale, 0) == pitch_classes[0]);
    assert(scale_pitch_class_at(scale, 1) == pitch_classes[1]);
    assert(scale_pitch_class_at(scale, 2) == pitch_classes[2]);

    scale_destroy(scale);

    /* An empty pitch-class set is not a Scale. */
    EntityID *tuning_system_id_2;
    assert(entity_id_create("tuning-12tet", &tuning_system_id_2) == ENTITY_ID_OK);
    Scale *empty;
    ScaleResult empty_result = scale_create(tuning_system_id_2, NULL, 0, &empty);
    assert(empty_result == SCALE_ERR_NULL_PITCH_CLASSES);
    entity_id_destroy(tuning_system_id_2);

    printf("test_scale: ok\n");
    return 0;
}