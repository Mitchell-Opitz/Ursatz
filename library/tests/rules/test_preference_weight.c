#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "rules/preference_weight.h"

/*
 * Exact-rational arithmetic for preference scores and weights: results are
 * reduced, nothing wraps around, a zero denominator and an overflow are
 * explicit results, and the failing call leaves its output untouched.
 */

static PreferenceFraction frac(unsigned long numerator, unsigned long denominator) {
    PreferenceFraction fraction = {numerator, denominator};
    return fraction;
}

static void assert_fraction(PreferenceFraction fraction, unsigned long numerator, unsigned long denominator) {
    assert(fraction.numerator == numerator && fraction.denominator == denominator);
}

static void test_reduce(void) {
    PreferenceFraction in = frac(2, 4), out = frac(9, 9);
    assert(preference_fraction_reduce(&in, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 2);

    in = frac(0, 5);
    assert(preference_fraction_reduce(&in, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 0, 1);

    in = frac(7, 3);
    assert(preference_fraction_reduce(&in, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 7, 3);

    in = frac(ULONG_MAX, ULONG_MAX);
    assert(preference_fraction_reduce(&in, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 1);

    out = frac(9, 9);
    in = frac(1, 0);
    assert(preference_fraction_reduce(&in, &out) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    assert_fraction(out, 9, 9);
    assert(preference_fraction_reduce(NULL, &out) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    in = frac(1, 2);
    assert(preference_fraction_reduce(&in, NULL) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
}

static void test_add(void) {
    PreferenceFraction a = frac(1, 2), b = frac(1, 3), out = frac(9, 9);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 5, 6);

    a = frac(1, 4);
    b = frac(1, 4);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 2);

    a = frac(0, 1);
    b = frac(3, 7);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 3, 7);

    /* Accumulating into one of the inputs. */
    a = frac(1, 6);
    b = frac(1, 3);
    assert(preference_fraction_add(&a, &b, &a) == PREFERENCE_WEIGHT_OK);
    assert_fraction(a, 1, 2);

    /* Large but exact: the sum reduces to a small fraction even though an intermediate would be large. */
    a = frac(ULONG_MAX - 1, ULONG_MAX);
    b = frac(1, ULONG_MAX);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 1);

    /* Overflow is an error, never a wrapped value, and the output is untouched. */
    out = frac(9, 9);
    a = frac(ULONG_MAX, 1);
    b = frac(1, 1);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_ERR_OVERFLOW);
    assert_fraction(out, 9, 9);
    a = frac(1, ULONG_MAX);
    b = frac(1, ULONG_MAX - 1);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_ERR_OVERFLOW);
    assert_fraction(out, 9, 9);

    a = frac(1, 0);
    b = frac(1, 2);
    assert(preference_fraction_add(&a, &b, &out) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    assert(preference_fraction_add(&b, &a, &out) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    assert(preference_fraction_add(NULL, &b, &out) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    assert(preference_fraction_add(&b, &b, NULL) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    assert_fraction(out, 9, 9);
}

static void test_multiply(void) {
    PreferenceFraction a = frac(2, 3), b = frac(3, 4), out = frac(9, 9);
    assert(preference_fraction_multiply(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 2);

    a = frac(0, 1);
    b = frac(5, 7);
    assert(preference_fraction_multiply(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 0, 1);

    /* Cancelling across first, so a product of large fractions that reduces to something small still works. */
    a = frac(ULONG_MAX, 3);
    b = frac(3, ULONG_MAX);
    assert(preference_fraction_multiply(&a, &b, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 1);

    out = frac(9, 9);
    a = frac(ULONG_MAX, 1);
    b = frac(2, 1);
    assert(preference_fraction_multiply(&a, &b, &out) == PREFERENCE_WEIGHT_ERR_OVERFLOW);
    a = frac(1, ULONG_MAX);
    b = frac(1, 2);
    assert(preference_fraction_multiply(&a, &b, &out) == PREFERENCE_WEIGHT_ERR_OVERFLOW);
    assert_fraction(out, 9, 9);

    a = frac(1, 2);
    b = frac(1, 0);
    assert(preference_fraction_multiply(&a, &b, &out) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    assert(preference_fraction_multiply(&a, NULL, &out) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
}

static int compare(PreferenceFraction a, PreferenceFraction b) {
    int comparison = 99;
    assert(preference_fraction_compare(&a, &b, &comparison) == PREFERENCE_WEIGHT_OK);
    int reverse = 99;
    assert(preference_fraction_compare(&b, &a, &reverse) == PREFERENCE_WEIGHT_OK);
    assert(reverse == -comparison);
    return comparison;
}

static void test_compare(void) {
    assert(compare(frac(1, 2), frac(1, 2)) == 0);
    assert(compare(frac(1, 2), frac(2, 4)) == 0);
    assert(compare(frac(0, 1), frac(0, 9)) == 0);
    assert(compare(frac(1, 3), frac(1, 2)) == -1);
    assert(compare(frac(2, 3), frac(3, 5)) == 1);
    assert(compare(frac(3, 2), frac(7, 5)) == 1);
    assert(compare(frac(1, 1), frac(0, 1)) == 1);
    assert(compare(frac(5, 3), frac(5, 4)) == 1);
    assert(compare(frac(7, 2), frac(7, 2)) == 0);
    assert(compare(frac(7, 2), frac(3, 1)) == 1);
    assert(compare(frac(3, 1), frac(7, 2)) == -1);

    /* Close fractions that would overflow a cross-multiplication. */
    assert(compare(frac(ULONG_MAX - 1, ULONG_MAX), frac(ULONG_MAX - 2, ULONG_MAX - 1)) == 1);
    assert(compare(frac(ULONG_MAX, ULONG_MAX), frac(ULONG_MAX - 1, ULONG_MAX)) == 1);
    assert(compare(frac(ULONG_MAX - 1, ULONG_MAX), frac(ULONG_MAX - 1, ULONG_MAX)) == 0);

    int comparison = 99;
    PreferenceFraction zero = frac(1, 0), one = frac(1, 1);
    assert(preference_fraction_compare(&zero, &one, &comparison) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    assert(preference_fraction_compare(&one, &zero, &comparison) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    assert(preference_fraction_compare(&one, &one, NULL) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    assert(preference_fraction_compare(NULL, &one, &comparison) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    assert(comparison == 99);
}

static void test_weighted_total(void) {
    PreferenceFraction out = frac(9, 9);

    /* (3 * 1/1 + 1 * 0/1) / 4 = 3/4. */
    PreferenceFraction scores[] = {frac(1, 1), frac(0, 1)};
    PreferenceFraction weights[] = {frac(3, 1), frac(1, 1)};
    assert(preference_fraction_weighted_total(scores, weights, 2, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 3, 4);

    /* Fractional weights: (1/2 * 1/2 + 1/3 * 1/1) / (1/2 + 1/3) = (7/12) / (5/6) = 7/10. */
    PreferenceFraction fs[] = {frac(1, 2), frac(1, 1)};
    PreferenceFraction fw[] = {frac(1, 2), frac(1, 3)};
    assert(preference_fraction_weighted_total(fs, fw, 2, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 7, 10);

    /* All best scores total 1/1, all worst total 0/1, whatever the weights. */
    PreferenceFraction best[] = {frac(1, 1), frac(2, 2), frac(5, 5)};
    PreferenceFraction worst[] = {frac(0, 1), frac(0, 3), frac(0, 7)};
    PreferenceFraction mixed[] = {frac(1, 3), frac(2, 5), frac(7, 11)};
    assert(preference_fraction_weighted_total(best, mixed, 3, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 1);
    assert(preference_fraction_weighted_total(worst, mixed, 3, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 0, 1);

    /* A zero weight drops its preference from the total. */
    PreferenceFraction zs[] = {frac(0, 1), frac(1, 1)};
    PreferenceFraction zw[] = {frac(0, 1), frac(2, 1)};
    assert(preference_fraction_weighted_total(zs, zw, 2, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 1, 1);

    /* Every weight zero, or no preferences at all: 0/1, not an error. */
    PreferenceFraction allzero[] = {frac(0, 1), frac(0, 4)};
    assert(preference_fraction_weighted_total(scores, allzero, 2, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 0, 1);
    out = frac(9, 9);
    assert(preference_fraction_weighted_total(NULL, NULL, 0, &out) == PREFERENCE_WEIGHT_OK);
    assert_fraction(out, 0, 1);

    /* A zero denominator anywhere is an error; so is an overflowing sum. */
    out = frac(9, 9);
    PreferenceFraction bad_scores[] = {frac(1, 1), frac(1, 0)};
    assert(preference_fraction_weighted_total(bad_scores, weights, 2, &out) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    PreferenceFraction bad_weights[] = {frac(1, 1), frac(1, 0)};
    assert(preference_fraction_weighted_total(scores, bad_weights, 2, &out) == PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR);
    PreferenceFraction big_weights[] = {frac(ULONG_MAX, 1), frac(1, 1)};
    assert(preference_fraction_weighted_total(scores, big_weights, 2, &out) == PREFERENCE_WEIGHT_ERR_OVERFLOW);
    assert_fraction(out, 9, 9);
    assert(preference_fraction_weighted_total(NULL, weights, 2, &out) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    assert(preference_fraction_weighted_total(scores, NULL, 2, &out) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
    assert(preference_fraction_weighted_total(scores, weights, 2, NULL) == PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT);
}

int main(void) {
    test_reduce();
    test_add();
    test_multiply();
    test_compare();
    test_weighted_total();
    printf("test_preference_weight: ok\n");
    return 0;
}
