#include <assert.h>
#include <limits.h>
#include <stddef.h>

#include "core/rational.h"

static Rational make(long long n, long long d) {
    Rational r;
    assert(rational_make(n, d, &r) == RATIONAL_OK);
    return r;
}

static void test_make_reduces_and_normalizes_sign(void) {
    Rational r = make(6, -8);
    assert(r.num == -3 && r.den == 4);
    r = make(0, -5);
    assert(r.num == 0 && r.den == 1);
    r = make(-4, -2);
    assert(r.num == 2 && r.den == 1);
}

static void test_make_errors_leave_out_untouched(void) {
    Rational r = {7, 9};
    assert(rational_make(1, 0, &r) == RATIONAL_ERR_ZERO_DENOMINATOR);
    assert(rational_make(LLONG_MIN, 1, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_make(1, LLONG_MIN, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_make(1, 2, NULL) == RATIONAL_ERR_NULL);
    assert(r.num == 7 && r.den == 9);
}

static void test_arithmetic(void) {
    Rational r;
    assert(rational_add(make(1, 2), make(1, 3), &r) == RATIONAL_OK && r.num == 5 && r.den == 6);
    assert(rational_sub(make(1, 3), make(1, 2), &r) == RATIONAL_OK && r.num == -1 && r.den == 6);
    assert(rational_mul(make(-2, 3), make(3, 4), &r) == RATIONAL_OK && r.num == -1 && r.den == 2);
    assert(rational_div(make(1, 2), make(-3, 4), &r) == RATIONAL_OK && r.num == -2 && r.den == 3);
    assert(rational_neg(make(1, 2), &r) == RATIONAL_OK && r.num == -1 && r.den == 2);
    assert(rational_mul_int(make(3, 4), 8, &r) == RATIONAL_OK && r.num == 6 && r.den == 1);
    assert(rational_div_int(make(3, 4), 6, &r) == RATIONAL_OK && r.num == 1 && r.den == 8);
    assert(rational_from_int(-5, &r) == RATIONAL_OK && r.num == -5 && r.den == 1);
}

static void test_zero_divisor(void) {
    Rational r = {7, 9};
    assert(rational_div(make(1, 2), make(0, 1), &r) == RATIONAL_ERR_ZERO_DENOMINATOR);
    assert(rational_div_int(make(1, 2), 0, &r) == RATIONAL_ERR_ZERO_DENOMINATOR);
    assert(r.num == 7 && r.den == 9);
}

static void test_overflow_leaves_out_untouched(void) {
    Rational r = {7, 9};
    Rational big = make(LLONG_MAX, 1);
    Rational tiny = make(1, LLONG_MAX);
    Rational other = make(1, LLONG_MAX - 1);
    assert(rational_add(big, make(1, 1), &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_add(tiny, other, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_sub(make(-LLONG_MAX, 1), make(1, 1), &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_mul(big, make(2, 1), &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_mul(tiny, other, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_div(tiny, big, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_mul_int(big, 2, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_mul_int(make(1, 2), LLONG_MIN, &r) == RATIONAL_ERR_OVERFLOW);
    assert(rational_div_int(tiny, 2, &r) == RATIONAL_ERR_OVERFLOW);
    assert(r.num == 7 && r.den == 9);
}

static void test_reduction_before_overflow(void) {
    Rational r;
    assert(rational_mul(make(LLONG_MAX, 2), make(2, LLONG_MAX), &r) == RATIONAL_OK && r.num == 1 && r.den == 1);
    assert(rational_add(make(1, LLONG_MAX), make(1, LLONG_MAX), &r) == RATIONAL_OK && r.num == 2 && r.den == LLONG_MAX);
}

static void test_floor_and_ceil_of_negatives(void) {
    long long v;
    assert(rational_floor(make(-7, 2), &v) == RATIONAL_OK && v == -4);
    assert(rational_ceil(make(-7, 2), &v) == RATIONAL_OK && v == -3);
    assert(rational_floor(make(7, 2), &v) == RATIONAL_OK && v == 3);
    assert(rational_ceil(make(7, 2), &v) == RATIONAL_OK && v == 4);
    assert(rational_floor(make(-4, 2), &v) == RATIONAL_OK && v == -2);
    assert(rational_ceil(make(-4, 2), &v) == RATIONAL_OK && v == -2);
    assert(rational_floor(make(LLONG_MIN + 1, 1), &v) == RATIONAL_OK && v == LLONG_MIN + 1);
}

static void test_cmp_never_overflows(void) {
    Rational a = make(LLONG_MAX, LLONG_MAX - 1);
    Rational b = make(LLONG_MAX - 1, LLONG_MAX - 2);
    assert(rational_cmp(a, b) < 0);
    assert(rational_cmp(b, a) > 0);
    assert(rational_cmp(a, a) == 0);
    assert(rational_cmp(make(-1, 3), make(1, 3)) < 0);
    assert(rational_cmp(make(1, 3), make(-1, 3)) > 0);
    assert(rational_cmp(make(-1, 3), make(-1, 2)) > 0);
    assert(rational_cmp(make(-LLONG_MAX, 1), make(LLONG_MAX, 1)) < 0);
    assert(rational_cmp(make(0, 1), make(0, 5)) == 0);
}

static void test_predicates_and_gcd(void) {
    assert(rational_equals(make(2, 4), make(1, 2)));
    assert(!rational_equals(make(1, 2), make(1, 3)));
    assert(rational_is_zero(make(0, 7)));
    assert(!rational_is_zero(make(1, 7)));
    assert(rational_is_negative(make(-1, 7)));
    assert(!rational_is_negative(make(0, 7)));
    assert(rational_gcd(12, 18) == 6);
    assert(rational_gcd(-12, 18) == 6);
    assert(rational_gcd(0, 0) == 0);
    assert(rational_gcd(0, -5) == 5);
    assert(rational_gcd(LLONG_MIN, 0) == LLONG_MAX);
    assert(rational_gcd_unsigned(ULLONG_MAX, ULLONG_MAX - 1) == 1);
}

static void test_unsigned_helpers(void) {
    unsigned long long v = 5;
    assert(!rational_checked_mul_unsigned(ULLONG_MAX, 2, &v) && v == 5);
    assert(rational_checked_mul_unsigned(1ULL << 32, 1ULL << 31, &v) && v == 1ULL << 63);
    assert(!rational_checked_add_unsigned(ULLONG_MAX, 1, &v) && v == 1ULL << 63);
    assert(rational_checked_add_unsigned(1, 2, &v) && v == 3);
    assert(rational_cmp_unsigned(1, ULLONG_MAX, 1, ULLONG_MAX - 1) < 0);
    Rational r = rational_ratio(6, 8);
    assert(r.num == 3 && r.den == 4);
    r = rational_ratio(1, 0);
    assert(r.num == 0 && r.den == 1);
}

int main(void) {
    test_make_reduces_and_normalizes_sign();
    test_make_errors_leave_out_untouched();
    test_arithmetic();
    test_zero_divisor();
    test_overflow_leaves_out_untouched();
    test_reduction_before_overflow();
    test_floor_and_ceil_of_negatives();
    test_cmp_never_overflows();
    test_predicates_and_gcd();
    test_unsigned_helpers();
    return 0;
}
