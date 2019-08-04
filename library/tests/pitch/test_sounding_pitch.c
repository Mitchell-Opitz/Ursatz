#include "pitch/sounding_pitch.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "identity/entity_id.h"

static TuningSystem *make_tuning_system(const char *id_value, const char *name) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    TuningSystem *tuning_system = NULL;
    tuning_system_create(id, name, &tuning_system);
    return tuning_system;
}

static PitchRealization *make_realization(TuningSystem *tuning_system, long numerator, unsigned long denominator) {
    PitchRealization *realization = NULL;
    pitch_realization_create(tuning_system, numerator, denominator, &realization);
    return realization;
}

static void test_create_rejects_null_realization(void) {
    SoundingPitch *sounding_pitch = NULL;
    SoundingPitchResult r = sounding_pitch_create(NULL, &sounding_pitch);
    assert(r == SOUNDING_PITCH_ERR_NULL_REALIZATION);
    assert(sounding_pitch == NULL);
}

static void test_create_accepts_valid(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    SoundingPitch *sounding_pitch = NULL;
    SoundingPitchResult r = sounding_pitch_create(make_realization(tuning_system, 3, 2), &sounding_pitch);
    assert(r == SOUNDING_PITCH_OK);
    assert(sounding_pitch != NULL);

    const PitchRealization *realization = sounding_pitch_realization(sounding_pitch);
    assert(pitch_realization_ratio_numerator(realization) == 3);
    assert(pitch_realization_ratio_denominator(realization) == 2);

    sounding_pitch_destroy(sounding_pitch);
    tuning_system_destroy(tuning_system);
}

static void test_equals_same_realization(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    SoundingPitch *a = NULL;
    SoundingPitch *b = NULL;
    sounding_pitch_create(make_realization(tuning_system, 3, 2), &a);
    sounding_pitch_create(make_realization(tuning_system, 6, 4), &b);

    assert(a != b);
    assert(sounding_pitch_equals(a, b));

    sounding_pitch_destroy(a);
    sounding_pitch_destroy(b);
    tuning_system_destroy(tuning_system);
}

static void test_inequals_different_realization(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    SoundingPitch *a = NULL;
    SoundingPitch *b = NULL;
    sounding_pitch_create(make_realization(tuning_system, 3, 2), &a);
    sounding_pitch_create(make_realization(tuning_system, 4, 3), &b);

    assert(!sounding_pitch_equals(a, b));

    sounding_pitch_destroy(a);
    sounding_pitch_destroy(b);
    tuning_system_destroy(tuning_system);
}

int main(void) {
    test_create_rejects_null_realization();
    test_create_accepts_valid();
    test_equals_same_realization();
    test_inequals_different_realization();
    printf("test_sounding_pitch: ok\n");
    return 0;
}