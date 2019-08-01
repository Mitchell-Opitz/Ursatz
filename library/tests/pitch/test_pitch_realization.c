#include "pitch/pitch_realization.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"

static TuningSystem *make_tuning_system(const char *id_value, const char *name) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    TuningSystem *tuning_system = NULL;
    tuning_system_create(id, name, &tuning_system);
    return tuning_system;
}

static void test_create_rejects_null_tuning_system(void) {
    PitchRealization *pr = NULL;
    PitchRealizationResult r = pitch_realization_create(NULL, 3, 2, &pr);
    assert(r == PITCH_REALIZATION_ERR_NULL_TUNING_SYSTEM);
    assert(pr == NULL);
}

static void test_create_rejects_non_positive_numerator(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *pr = NULL;
    PitchRealizationResult r = pitch_realization_create(tuning_system, 0, 2, &pr);
    assert(r == PITCH_REALIZATION_ERR_NON_POSITIVE_NUMERATOR);
    assert(pr == NULL);

    r = pitch_realization_create(tuning_system, -3, 2, &pr);
    assert(r == PITCH_REALIZATION_ERR_NON_POSITIVE_NUMERATOR);
    assert(pr == NULL);

    tuning_system_destroy(tuning_system);
}

static void test_create_rejects_zero_denominator(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *pr = NULL;
    PitchRealizationResult r = pitch_realization_create(tuning_system, 3, 0, &pr);
    assert(r == PITCH_REALIZATION_ERR_NON_POSITIVE_DENOMINATOR);
    assert(pr == NULL);

    tuning_system_destroy(tuning_system);
}

static void test_create_reduces_to_lowest_terms(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *pr = NULL;
    pitch_realization_create(tuning_system, 6, 4, &pr);
    assert(pitch_realization_ratio_numerator(pr) == 3);
    assert(pitch_realization_ratio_denominator(pr) == 2);

    pitch_realization_destroy(pr);
    tuning_system_destroy(tuning_system);
}

/* PitchRealization copies the TuningSystem it's given: destroying the
   caller's original does not invalidate the stored reference. */
static void test_does_not_alias_tuning_system(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *pr = NULL;
    pitch_realization_create(tuning_system, 3, 2, &pr);

    tuning_system_destroy(tuning_system);

    assert(strcmp(tuning_system_name(pitch_realization_tuning_system(pr)), "12-TET") == 0);

    pitch_realization_destroy(pr);
}

static void test_equals_same_tuning_system_and_ratio(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *a = NULL;
    PitchRealization *b = NULL;
    pitch_realization_create(tuning_system, 3, 2, &a);
    pitch_realization_create(tuning_system, 6, 4, &b);

    assert(a != b);
    assert(pitch_realization_equals(a, b));

    pitch_realization_destroy(a);
    pitch_realization_destroy(b);
    tuning_system_destroy(tuning_system);
}

static void test_inequals_different_ratio(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *a = NULL;
    PitchRealization *b = NULL;
    pitch_realization_create(tuning_system, 3, 2, &a);
    pitch_realization_create(tuning_system, 4, 3, &b);

    assert(!pitch_realization_equals(a, b));

    pitch_realization_destroy(a);
    pitch_realization_destroy(b);
    tuning_system_destroy(tuning_system);
}

static void test_inequals_different_tuning_system(void) {
    TuningSystem *tuning_a = make_tuning_system("tuning-12tet", "12-TET");
    TuningSystem *tuning_b = make_tuning_system("tuning-ji", "just intonation");

    PitchRealization *a = NULL;
    PitchRealization *b = NULL;
    pitch_realization_create(tuning_a, 3, 2, &a);
    pitch_realization_create(tuning_b, 3, 2, &b);

    assert(!pitch_realization_equals(a, b));

    pitch_realization_destroy(a);
    pitch_realization_destroy(b);
    tuning_system_destroy(tuning_a);
    tuning_system_destroy(tuning_b);
}

/* Known-value round-trip: a perfect fifth (3/2) against a reference of
   440/1 (A440) yields an exact frequency of 660/1, entirely in rational
   arithmetic -- no float/double anywhere (Law 19). */
static void test_frequency_round_trips_known_value(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *pr = NULL;
    pitch_realization_create(tuning_system, 3, 2, &pr);

    long freq_numerator = 0;
    unsigned long freq_denominator = 0;
    PitchRealizationFrequencyResult r =
        pitch_realization_frequency(pr, 440, 1, &freq_numerator, &freq_denominator);
    assert(r == PITCH_REALIZATION_FREQUENCY_OK);
    assert(freq_numerator == 660);
    assert(freq_denominator == 1);

    pitch_realization_destroy(pr);
    tuning_system_destroy(tuning_system);
}

static void test_frequency_rejects_zero_reference_denominator(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchRealization *pr = NULL;
    pitch_realization_create(tuning_system, 3, 2, &pr);

    long freq_numerator = 0;
    unsigned long freq_denominator = 0;
    PitchRealizationFrequencyResult r = pitch_realization_frequency(pr, 440, 0, &freq_numerator, &freq_denominator);
    assert(r == PITCH_REALIZATION_FREQUENCY_ERR_NON_POSITIVE_REFERENCE_DENOMINATOR);

    pitch_realization_destroy(pr);
    tuning_system_destroy(tuning_system);
}

int main(void) {
    test_create_rejects_null_tuning_system();
    test_create_rejects_non_positive_numerator();
    test_create_rejects_zero_denominator();
    test_create_reduces_to_lowest_terms();
    test_does_not_alias_tuning_system();
    test_equals_same_tuning_system_and_ratio();
    test_inequals_different_ratio();
    test_inequals_different_tuning_system();
    test_frequency_round_trips_known_value();
    test_frequency_rejects_zero_reference_denominator();
    printf("test_pitch_realization: ok\n");
    return 0;
}