#include "time/time_instant.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_zero_denominator(void) {
    TimeInstant *instant = NULL;
    TimeInstantResult r = time_instant_create(1, 0, &instant);
    assert(r == TIME_INSTANT_ERR_NON_POSITIVE_DENOMINATOR);
    assert(instant == NULL);
}

static void test_create_accepts_valid(void) {
    TimeInstant *instant = NULL;
    TimeInstantResult r = time_instant_create(3, 4, &instant);
    assert(r == TIME_INSTANT_OK);
    assert(instant != NULL);
    assert(time_instant_numerator(instant) == 3);
    assert(time_instant_denominator(instant) == 4);
    time_instant_destroy(instant);
}

/* No lower bound: negative instants (e.g. pickup measures) are legal. */
static void test_create_accepts_negative_numerator(void) {
    TimeInstant *instant = NULL;
    TimeInstantResult r = time_instant_create(-1, 4, &instant);
    assert(r == TIME_INSTANT_OK);
    assert(time_instant_numerator(instant) == -1);
    assert(time_instant_denominator(instant) == 4);
    time_instant_destroy(instant);
}

static void test_create_reduces_to_lowest_terms(void) {
    TimeInstant *instant = NULL;
    time_instant_create(6, 8, &instant);
    assert(time_instant_numerator(instant) == 3);
    assert(time_instant_denominator(instant) == 4);
    time_instant_destroy(instant);
}

/* Property: TimeInstant(a,b) == TimeInstant(k*a, k*b) for any positive k. */
static void test_equality_scale_invariant(void) {
    TimeInstant *base = NULL;
    time_instant_create(-3, 4, &base);

    for (unsigned long k = 1; k <= 20; k++) {
        TimeInstant *scaled = NULL;
        time_instant_create(-3L * (long)k, 4UL * k, &scaled);
        assert(time_instant_equals(base, scaled));
        time_instant_destroy(scaled);
    }

    time_instant_destroy(base);
}

static void test_inequality_different_reduced_value(void) {
    TimeInstant *a = NULL;
    TimeInstant *b = NULL;
    time_instant_create(1, 3, &a);
    time_instant_create(1, 4, &b);

    assert(!time_instant_equals(a, b));

    time_instant_destroy(a);
    time_instant_destroy(b);
}

static void test_time_accessor_matches_components(void) {
    TimeInstant *instant = NULL;
    time_instant_create(1, 4, &instant);

    const MusicalTime *time = time_instant_time(instant);
    assert(musical_time_numerator(time) == 1);
    assert(musical_time_denominator(time) == 4);

    time_instant_destroy(instant);
}

int main(void) {
    test_create_rejects_zero_denominator();
    test_create_accepts_valid();
    test_create_accepts_negative_numerator();
    test_create_reduces_to_lowest_terms();
    test_equality_scale_invariant();
    test_inequality_different_reduced_value();
    test_time_accessor_matches_components();
    return 0;
}