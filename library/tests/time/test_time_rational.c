#include <assert.h>
#include <limits.h>
#include <stddef.h>

#include "core/rational.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/performance_time.h"
#include "time/time_rational.h"

static void test_musical_time_round_trip(void) {
    MusicalTime *t = NULL;
    assert(musical_time_create(3, 4, &t) == MUSICAL_TIME_OK);
    Rational r;
    assert(rational_of_musical_time(t, &r) == RATIONAL_OK && r.num == 3 && r.den == 4);
    MusicalTime *back = NULL;
    assert(musical_time_from_rational(r, &back) == TIME_RATIONAL_OK);
    assert(musical_time_equals(t, back));
    musical_time_destroy(t);
    musical_time_destroy(back);
}

static void test_negative_musical_time(void) {
    MusicalTime *t = NULL;
    assert(musical_time_create(-3, 4, &t) == MUSICAL_TIME_OK);
    Rational r;
    assert(rational_of_musical_time(t, &r) == RATIONAL_OK && r.num == -3 && r.den == 4);
    MusicalTime *back = NULL;
    assert(musical_time_from_rational(r, &back) == TIME_RATIONAL_OK && musical_time_equals(t, back));
    musical_time_destroy(t);
    musical_time_destroy(back);
}

static void test_duration_round_trip_and_negative(void) {
    Duration *d = NULL;
    assert(duration_create(5, 8, &d) == DURATION_OK);
    Rational r;
    assert(rational_of_duration(d, &r) == RATIONAL_OK && r.num == 5 && r.den == 8);
    Duration *back = NULL;
    assert(duration_from_rational(r, &back) == TIME_RATIONAL_OK && duration_equals(d, back));
    duration_destroy(d);
    duration_destroy(back);

    Duration *none = NULL;
    Rational negative = {-1, 2};
    assert(duration_from_rational(negative, &none) == TIME_RATIONAL_ERR_NEGATIVE);
    assert(none == NULL);
}

static void test_performance_time_reads(void) {
    PerformanceTime *p = NULL;
    assert(performance_time_create(7, 2, &p) == PERFORMANCE_TIME_OK);
    Rational r;
    assert(rational_of_performance_time(p, &r) == RATIONAL_OK && r.num == 7 && r.den == 2);
    performance_time_destroy(p);
}

static void test_does_not_fit(void) {
    /* A denominator above LLONG_MAX does not fit a Rational. */
    MusicalTime *wide = NULL;
    assert(musical_time_create(1, ULONG_MAX, &wide) == MUSICAL_TIME_OK);
    Rational r = {7, 9};
    assert(rational_of_musical_time(wide, &r) == RATIONAL_ERR_OVERFLOW);
    assert(r.num == 7 && r.den == 9);
    musical_time_destroy(wide);

    /* A denominator above ULONG_MAX does not fit a MusicalTime (only when long is narrower). */
    assert(rational_of_musical_time(NULL, &r) == RATIONAL_ERR_NULL);
    MusicalTime *none = NULL;
    assert(musical_time_from_rational(r, NULL) == TIME_RATIONAL_ERR_NULL);
    assert(none == NULL);
}

static void test_musical_time_add_overflow(void) {
    MusicalTime *t = NULL;
    Duration *d = NULL;
    assert(musical_time_create(LONG_MAX, 1, &t) == MUSICAL_TIME_OK);
    assert(duration_create(1, 1, &d) == DURATION_OK);
    MusicalTime *sum = NULL;
    assert(musical_time_add(t, d, &sum) == MUSICAL_TIME_ERR_OVERFLOW);
    assert(sum == NULL);
    musical_time_destroy(t);
    duration_destroy(d);
}

static void test_duration_arithmetic_overflow(void) {
    Duration *a = NULL;
    Duration *b = NULL;
    Duration *out = NULL;
    assert(duration_create(LONG_MAX, 1, &a) == DURATION_OK);
    assert(duration_create(1, 1, &b) == DURATION_OK);
    assert(duration_add(a, b, &out) == DURATION_ERR_OVERFLOW);
    assert(out == NULL);
    assert(duration_subtract(b, a, &out) == DURATION_ERR_NEGATIVE);
    assert(out == NULL);
    duration_destroy(a);
    duration_destroy(b);
}

static void test_musical_time_subtract_overflow(void) {
    MusicalTime *a = NULL;
    MusicalTime *b = NULL;
    assert(musical_time_create(1, LONG_MAX, &a) == MUSICAL_TIME_OK);
    assert(musical_time_create(1, LONG_MAX - 1, &b) == MUSICAL_TIME_OK);
    Duration *out = NULL;
    assert(musical_time_subtract(a, b, &out) == DURATION_ERR_OVERFLOW);
    assert(out == NULL);
    musical_time_destroy(a);
    musical_time_destroy(b);
}

static void test_exact_results_unchanged(void) {
    MusicalTime *t = NULL;
    Duration *d = NULL;
    MusicalTime *sum = NULL;
    assert(musical_time_create(1, 3, &t) == MUSICAL_TIME_OK);
    assert(duration_create(1, 6, &d) == DURATION_OK);
    assert(musical_time_add(t, d, &sum) == MUSICAL_TIME_OK);
    assert(musical_time_numerator(sum) == 1 && musical_time_denominator(sum) == 2);
    musical_time_destroy(t);
    musical_time_destroy(sum);
    duration_destroy(d);
}

int main(void) {
    test_musical_time_round_trip();
    test_negative_musical_time();
    test_duration_round_trip_and_negative();
    test_performance_time_reads();
    test_does_not_fit();
    test_musical_time_add_overflow();
    test_duration_arithmetic_overflow();
    test_musical_time_subtract_overflow();
    test_exact_results_unchanged();
    return 0;
}
