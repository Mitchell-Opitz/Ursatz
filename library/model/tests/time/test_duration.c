#include "model/time/duration.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

static Duration du(long long n, long long d) {
    Duration x;
    assert(duration_make(n, d, &x) == DURATION_OK);
    return x;
}

static void test_make_rejects_zero_denominator(void) {
    Duration keep = du(1, 2);
    Duration d = keep;
    DurationResult r = duration_make(1, 0, &d);
    assert(r == DURATION_ERR_NON_POSITIVE_DENOMINATOR);
    assert(duration_same(d, keep));
}

static void test_make_rejects_negative_numerator(void) {
    Duration keep = du(1, 2);
    Duration d = keep;
    DurationResult r = duration_make(-1, 4, &d);
    assert(r == DURATION_ERR_NEGATIVE);
    assert(duration_same(d, keep));
}

static void test_make_accepts_valid(void) {
    Duration d;
    DurationResult r = duration_make(3, 4, &d);
    assert(r == DURATION_OK);
    assert(d.value.num == 3);
    assert(d.value.den == 4);
}

static void test_make_reduces_to_lowest_terms(void) {
    Duration d = du(6, 8);
    assert(d.value.num == 3);
    assert(d.value.den == 4);
}

static void test_make_reduces_zero_numerator(void) {
    Duration d = du(0, 5);
    assert(d.value.num == 0);
    assert(d.value.den == 1);
}

/* A denominator that does not fit is a refusal, not a wrapped value. */
static void test_make_refuses_what_does_not_fit(void) {
    Duration keep = du(1, 2);
    Duration d = keep;
    assert(duration_make(LLONG_MIN, 1, &d) == DURATION_ERR_OVERFLOW);
    assert(duration_same(d, keep));
}

/* Property: Duration(a,b) == Duration(k*a, k*b) for any positive integer k. */
static void test_equality_scale_invariant(void) {
    Duration base = du(3, 4);

    for (long long k = 1; k <= 20; k++) {
        Duration scaled = du(3 * k, 4 * k);
        assert(duration_same(base, scaled));
        assert(rational_cmp(base.value, scaled.value) == 0);
    }
}

/* Property: no constructor path yields a negative duration. */
static void test_never_negative(void) {
    for (long long numerator = -5; numerator <= 5; numerator++) {
        Duration keep = du(1, 8);
        Duration d = keep;
        DurationResult r = duration_make(numerator, 4, &d);
        if (numerator < 0) {
            assert(r == DURATION_ERR_NEGATIVE);
            assert(duration_same(d, keep));
        } else {
            assert(r == DURATION_OK);
            assert(d.value.num >= 0);
        }
    }
}

static void test_inequality_different_reduced_value(void) {
    Duration a = du(1, 3);
    Duration b = du(1, 4);

    assert(!duration_same(a, b));
    assert(rational_cmp(a.value, b.value) > 0);
}

static void test_add(void) {
    Duration sum;
    DurationResult r = duration_plus(du(1, 4), du(1, 4), &sum);
    assert(r == DURATION_OK);
    assert(sum.value.num == 1);
    assert(sum.value.den == 2);
}

int main(void) {
    test_make_rejects_zero_denominator();
    test_make_rejects_negative_numerator();
    test_make_accepts_valid();
    test_make_reduces_to_lowest_terms();
    test_make_reduces_zero_numerator();
    test_make_refuses_what_does_not_fit();
    test_equality_scale_invariant();
    test_never_negative();
    test_inequality_different_reduced_value();
    test_add();
    return 0;
}
