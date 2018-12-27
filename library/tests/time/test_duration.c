#include "time/duration.h"

#include <assert.h>
#include <stddef.h>

static void test_create_rejects_zero_denominator(void) {
    Duration *d = NULL;
    DurationResult r = duration_create(1, 0, &d);
    assert(r == DURATION_ERR_NON_POSITIVE_DENOMINATOR);
    assert(d == NULL);
}

static void test_create_rejects_negative_numerator(void) {
    Duration *d = NULL;
    DurationResult r = duration_create(-1, 4, &d);
    assert(r == DURATION_ERR_NEGATIVE);
    assert(d == NULL);
}

static void test_create_accepts_valid(void) {
    Duration *d = NULL;
    DurationResult r = duration_create(3, 4, &d);
    assert(r == DURATION_OK);
    assert(d != NULL);
    assert(duration_numerator(d) == 3);
    assert(duration_denominator(d) == 4);
    duration_destroy(d);
}

static void test_create_reduces_to_lowest_terms(void) {
    Duration *d = NULL;
    duration_create(6, 8, &d);
    assert(duration_numerator(d) == 3);
    assert(duration_denominator(d) == 4);
    duration_destroy(d);
}

static void test_create_reduces_zero_numerator(void) {
    Duration *d = NULL;
    duration_create(0, 5, &d);
    assert(duration_numerator(d) == 0);
    assert(duration_denominator(d) == 1);
    duration_destroy(d);
}

/* Property: Duration(a,b) == Duration(k*a, k*b) for any positive integer k. */
static void test_equality_scale_invariant(void) {
    Duration *base = NULL;
    duration_create(3, 4, &base);

    for (unsigned long k = 1; k <= 20; k++) {
        Duration *scaled = NULL;
        duration_create(3L * (long)k, 4UL * k, &scaled);
        assert(duration_equals(base, scaled));
        duration_destroy(scaled);
    }

    duration_destroy(base);
}

/* Property: no constructor path yields a negative duration. */
static void test_never_negative(void) {
    for (long numerator = -5; numerator <= 5; numerator++) {
        Duration *d = NULL;
        DurationResult r = duration_create(numerator, 4, &d);
        if (numerator < 0) {
            assert(r == DURATION_ERR_NEGATIVE);
            assert(d == NULL);
        } else {
            assert(r == DURATION_OK);
            assert(duration_numerator(d) >= 0);
            duration_destroy(d);
        }
    }
}

static void test_inequality_different_reduced_value(void) {
    Duration *a = NULL;
    Duration *b = NULL;
    duration_create(1, 3, &a);
    duration_create(1, 4, &b);

    assert(!duration_equals(a, b));

    duration_destroy(a);
    duration_destroy(b);
}

static void test_add(void) {
    Duration *a = NULL;
    Duration *b = NULL;
    Duration *sum = NULL;
    duration_create(1, 4, &a);
    duration_create(1, 4, &b);

    DurationResult r = duration_add(a, b, &sum);
    assert(r == DURATION_OK);
    assert(duration_numerator(sum) == 1);
    assert(duration_denominator(sum) == 2);

    duration_destroy(a);
    duration_destroy(b);
    duration_destroy(sum);
}

static void test_subtract(void) {
    Duration *a = NULL;
    Duration *b = NULL;
    Duration *diff = NULL;
    duration_create(3, 4, &a);
    duration_create(1, 4, &b);

    DurationResult r = duration_subtract(a, b, &diff);
    assert(r == DURATION_OK);
    assert(duration_numerator(diff) == 1);
    assert(duration_denominator(diff) == 2);

    duration_destroy(a);
    duration_destroy(b);
    duration_destroy(diff);
}

static void test_subtract_rejects_negative_result(void) {
    Duration *a = NULL;
    Duration *b = NULL;
    Duration *diff = NULL;
    duration_create(1, 4, &a);
    duration_create(3, 4, &b);

    DurationResult r = duration_subtract(a, b, &diff);
    assert(r == DURATION_ERR_NEGATIVE);
    assert(diff == NULL);

    duration_destroy(a);
    duration_destroy(b);
}

int main(void) {
    test_create_rejects_zero_denominator();
    test_create_rejects_negative_numerator();
    test_create_accepts_valid();
    test_create_reduces_to_lowest_terms();
    test_create_reduces_zero_numerator();
    test_equality_scale_invariant();
    test_never_negative();
    test_inequality_different_reduced_value();
    test_add();
    test_subtract();
    test_subtract_rejects_negative_result();
    return 0;
}