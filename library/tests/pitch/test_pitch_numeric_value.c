#include "pitch/pitch_numeric_value.h"

#include <assert.h>
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

static void test_equals_same_tuning_system_and_position(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *a = NULL;
    PitchNumericValue *b = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &a);
    pitch_numeric_value_create(tuning_system, 120, 2, &b);

    assert(a != b);
    assert(pitch_numeric_value_equals(a, b));

    pitch_numeric_value_destroy(a);
    pitch_numeric_value_destroy(b);
    tuning_system_destroy(tuning_system);
}

static void test_inequals_different_tuning_system(void) {
    TuningSystem *tuning_a = make_tuning_system("tuning-12tet", "12-TET");
    TuningSystem *tuning_b = make_tuning_system("tuning-24tet", "24-TET");

    PitchNumericValue *a = NULL;
    PitchNumericValue *b = NULL;
    pitch_numeric_value_create(tuning_a, 60, 1, &a);
    pitch_numeric_value_create(tuning_b, 60, 1, &b);

    assert(!pitch_numeric_value_equals(a, b));

    pitch_numeric_value_destroy(a);
    pitch_numeric_value_destroy(b);
    tuning_system_destroy(tuning_a);
    tuning_system_destroy(tuning_b);
}

/* Transposition's core operation: middle C (60/1) up a major third
   (4 semitones, 4/1) = 64/1, exact rational, no float/double. */
static void test_add_interval_computes_exact_rational_shift(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *base = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &base);

    PitchNumericValue *shifted = NULL;
    PitchNumericValueOpResult r = pitch_numeric_value_add_interval(base, 4, 1, &shifted);
    assert(r == PITCH_NUMERIC_VALUE_OP_OK);
    assert(pitch_numeric_value_position_numerator(shifted) == 64);
    assert(pitch_numeric_value_position_denominator(shifted) == 1);

    /* A downward (negative) interval is also supported. */
    PitchNumericValue *down = NULL;
    r = pitch_numeric_value_add_interval(base, -12, 1, &down);
    assert(r == PITCH_NUMERIC_VALUE_OP_OK);
    assert(pitch_numeric_value_position_numerator(down) == 48);

    pitch_numeric_value_destroy(base);
    pitch_numeric_value_destroy(shifted);
    pitch_numeric_value_destroy(down);
    tuning_system_destroy(tuning_system);
}

static void test_add_interval_rejects_zero_denominator(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *base = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &base);

    PitchNumericValue *out = NULL;
    PitchNumericValueOpResult r = pitch_numeric_value_add_interval(base, 4, 0, &out);
    assert(r == PITCH_NUMERIC_VALUE_OP_ERR_NON_POSITIVE_DENOMINATOR);

    pitch_numeric_value_destroy(base);
    tuning_system_destroy(tuning_system);
}

/* Inversion's core operation: reflecting 64/1 about axis 60/1 gives
   2*60 - 64 = 56/1. */
static void test_reflect_about_axis_computes_exact_reflection(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *axis = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &axis);
    PitchNumericValue *value = NULL;
    pitch_numeric_value_create(tuning_system, 64, 1, &value);

    PitchNumericValue *reflected = NULL;
    PitchNumericValueOpResult r = pitch_numeric_value_reflect_about_axis(value, axis, &reflected);
    assert(r == PITCH_NUMERIC_VALUE_OP_OK);
    assert(pitch_numeric_value_position_numerator(reflected) == 56);
    assert(pitch_numeric_value_position_denominator(reflected) == 1);

    pitch_numeric_value_destroy(axis);
    pitch_numeric_value_destroy(value);
    pitch_numeric_value_destroy(reflected);
    tuning_system_destroy(tuning_system);
}

static void test_reflect_about_axis_rejects_tuning_system_mismatch(void) {
    TuningSystem *tuning_a = make_tuning_system("tuning-12tet", "12-TET");
    TuningSystem *tuning_b = make_tuning_system("tuning-24tet", "24-TET");

    PitchNumericValue *axis = NULL;
    pitch_numeric_value_create(tuning_a, 60, 1, &axis);
    PitchNumericValue *value = NULL;
    pitch_numeric_value_create(tuning_b, 64, 1, &value);

    PitchNumericValue *out = NULL;
    PitchNumericValueOpResult r = pitch_numeric_value_reflect_about_axis(value, axis, &out);
    assert(r == PITCH_NUMERIC_VALUE_OP_ERR_TUNING_SYSTEM_MISMATCH);

    pitch_numeric_value_destroy(axis);
    pitch_numeric_value_destroy(value);
    tuning_system_destroy(tuning_a);
    tuning_system_destroy(tuning_b);
}

static void test_distance_computes_exact_difference(void) {
    TuningSystem *tuning_system = make_tuning_system("tuning-12tet", "12-TET");

    PitchNumericValue *a = NULL;
    pitch_numeric_value_create(tuning_system, 64, 1, &a);
    PitchNumericValue *b = NULL;
    pitch_numeric_value_create(tuning_system, 60, 1, &b);

    long out_numerator = 0;
    unsigned long out_denominator = 0;
    PitchNumericValueOpResult r = pitch_numeric_value_distance(a, b, &out_numerator, &out_denominator);
    assert(r == PITCH_NUMERIC_VALUE_OP_OK);
    assert(out_numerator == 4);
    assert(out_denominator == 1);

    pitch_numeric_value_destroy(a);
    pitch_numeric_value_destroy(b);
    tuning_system_destroy(tuning_system);
}

static void test_distance_rejects_tuning_system_mismatch(void) {
    TuningSystem *tuning_a = make_tuning_system("tuning-12tet", "12-TET");
    TuningSystem *tuning_b = make_tuning_system("tuning-24tet", "24-TET");

    PitchNumericValue *a = NULL;
    pitch_numeric_value_create(tuning_a, 64, 1, &a);
    PitchNumericValue *b = NULL;
    pitch_numeric_value_create(tuning_b, 60, 1, &b);

    long out_numerator = 0;
    unsigned long out_denominator = 0;
    PitchNumericValueOpResult r = pitch_numeric_value_distance(a, b, &out_numerator, &out_denominator);
    assert(r == PITCH_NUMERIC_VALUE_OP_ERR_TUNING_SYSTEM_MISMATCH);

    pitch_numeric_value_destroy(a);
    pitch_numeric_value_destroy(b);
    tuning_system_destroy(tuning_a);
    tuning_system_destroy(tuning_b);
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
    assert(pitch_identity_equals(identity_a, identity_b));

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

    assert(!pitch_identity_equals(identity_a, identity_b));

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
    test_equals_same_tuning_system_and_position();
    test_inequals_different_tuning_system();
    test_add_interval_computes_exact_rational_shift();
    test_add_interval_rejects_zero_denominator();
    test_reflect_about_axis_computes_exact_reflection();
    test_reflect_about_axis_rejects_tuning_system_mismatch();
    test_distance_computes_exact_difference();
    test_distance_rejects_tuning_system_mismatch();
    test_to_identity_is_deterministic_and_usable();
    test_to_identity_differs_for_different_positions();
    printf("test_pitch_numeric_value: ok\n");
    return 0;
}