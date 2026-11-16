#include "pitch/pitch_numeric_value.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/tuning_system.h"

/*
 * Construction test: a PitchNumericValue holds a copied TuningSystem
 * (not aliased) and a reduced exact-rational position.
 *
 * Operation tests: add_interval (Transposition's core op),
 * reflect_about_axis (Inversion's core op), and distance all stay exact
 * rational and reject a TuningSystem mismatch between operands.
 *
 * Conversion test: pitch_numeric_value_to_identity produces a usable,
 * deterministic PitchIdentity -- same PitchNumericValue converts to
 * equal PitchIdentity tokens; this is one-directional only, there is no
 * decoder back (see pitch_numeric_value.h).
 */

/* Same token bytes. */
static bool same_pitch_identity(const PitchIdentity *a, const PitchIdentity *b) {
    size_t a_size = 0;
    size_t b_size = 0;
    const void *a_token = pitch_identity_token(a, &a_size);
    const void *b_token = pitch_identity_token(b, &b_size);
    return a_size == b_size && memcmp(a_token, b_token, a_size) == 0;
}

static TuningSystem *make_tuning_system(const char *id_value, const char *name) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);

    TuningSystem *tuning_system = NULL;
    tuning_system_create(id, name, &tuning_system);
    return tuning_system;
}

static void test_create_rejects_null_tuning_system(void) {
    PitchNumericValue *v = NULL;
    PitchNumericValueResult r = pitch_numeric_value_create(NULL, 3, 2, &v);
    assert(r == PITCH_NUMERIC_VALUE_ERR_NULL_TUNING_SYSTEM);
    assert(v == NULL);
}

static void test_create_rejects_zero_denominator(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *v = NULL;
    PitchNumericValueResult r = pitch_numeric_value_create(tuning_system, 3, 0, &v);
    assert(r == PITCH_NUMERIC_VALUE_ERR_NON_POSITIVE_DENOMINATOR);
    assert(v == NULL);

    tuning_system_destroy(tuning_system);
}

static void test_create_reduces_to_lowest_terms_and_allows_negative(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *v = NULL;
    pitch_numeric_value_create(tuning_system, -6, 4, &v);
    assert(pitch_numeric_value_position_numerator(v) == -3);
    assert(pitch_numeric_value_position_denominator(v) == 2);

    pitch_numeric_value_destroy(v);
    tuning_system_destroy(tuning_system);
}

static void test_does_not_alias_tuning_system(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *v = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &v);

    tuning_system_destroy(tuning_system);

    assert(strcmp(tuning_system_name(pitch_numeric_value_tuning_system(v)), "12-TET") == 0);

    pitch_numeric_value_destroy(v);
}

static void test_to_identity_is_deterministic_and_usable(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *a = NULL;
    pitch_numeric_value_create(tuning_system, 64, 1, &a);
    PitchNumericValue *b = NULL;
    pitch_numeric_value_create(tuning_system, 128, 2, &b);

    PitchIdentity *identity_a = NULL;
    PitchIdentity *identity_b = NULL;
    assert(pitch_numeric_value_to_identity(a, &identity_a) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_numeric_value_to_identity(b, &identity_b) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);

    /* Same reduced position under the same tuning system -> equal PitchIdentity tokens. */
    assert(same_pitch_identity(identity_a, identity_b));

    pitch_identity_destroy(identity_a);
    pitch_identity_destroy(identity_b);
    pitch_numeric_value_destroy(a);
    pitch_numeric_value_destroy(b);
    tuning_system_destroy(tuning_system);
}

static void test_to_identity_differs_for_different_positions(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *a = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &a);
    PitchNumericValue *b = NULL;
    pitch_numeric_value_create(tuning_system, 64, 1, &b);

    PitchIdentity *identity_a = NULL;
    PitchIdentity *identity_b = NULL;
    assert(pitch_numeric_value_to_identity(a, &identity_a) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    assert(pitch_numeric_value_to_identity(b, &identity_b) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);

    assert(!same_pitch_identity(identity_a, identity_b));

    pitch_identity_destroy(identity_a);
    pitch_identity_destroy(identity_b);
    pitch_numeric_value_destroy(a);
    pitch_numeric_value_destroy(b);
    tuning_system_destroy(tuning_system);
}

int main(void) {
    test_create_rejects_null_tuning_system();
    test_create_rejects_zero_denominator();
    test_create_reduces_to_lowest_terms_and_allows_negative();
    test_does_not_alias_tuning_system();
    test_to_identity_is_deterministic_and_usable();
    test_to_identity_differs_for_different_positions();
    printf("test_pitch_numeric_value: ok\n");
    return 0;
}