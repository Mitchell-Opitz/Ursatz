#include "theory/style_fraction.h"

#include <limits.h>
#include <stddef.h>

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

/* 0 (and *out untouched) when a * b does not fit. */
static int checked_multiply(unsigned long a, unsigned long b, unsigned long *out) {
    if (a != 0 && b > ULONG_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static int checked_add(unsigned long a, unsigned long b, unsigned long *out) {
    if (b > ULONG_MAX - a) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static StyleFractionResult validate(const StyleFraction *fraction) {
    if (fraction == NULL) {
        return STYLE_FRACTION_ERR_NULL_ARGUMENT;
    }
    return fraction->denominator == 0 ? STYLE_FRACTION_ERR_ZERO_DENOMINATOR : STYLE_FRACTION_OK;
}

static StyleFraction reduced(unsigned long numerator, unsigned long denominator) {
    unsigned long divisor = gcd(numerator, denominator);
    StyleFraction result = {numerator / divisor, denominator / divisor};
    return result;
}

StyleFractionResult style_fraction_reduce(const StyleFraction *fraction, StyleFraction *out) {
    StyleFractionResult status = validate(fraction);
    if (status != STYLE_FRACTION_OK) {
        return status;
    }
    if (out == NULL) {
        return STYLE_FRACTION_ERR_NULL_ARGUMENT;
    }
    *out = reduced(fraction->numerator, fraction->denominator);
    return STYLE_FRACTION_OK;
}

StyleFractionResult style_fraction_add(const StyleFraction *a, const StyleFraction *b, StyleFraction *out) {
    StyleFractionResult status = validate(a);
    if (status == STYLE_FRACTION_OK) {
        status = validate(b);
    }
    if (status != STYLE_FRACTION_OK) {
        return status;
    }
    if (out == NULL) {
        return STYLE_FRACTION_ERR_NULL_ARGUMENT;
    }
    /* Over the least common denominator: a/(g*x) + b/(g*y) = (a*y + b*x) / (g*x*y). */
    unsigned long divisor = gcd(a->denominator, b->denominator);
    unsigned long x = a->denominator / divisor;
    unsigned long y = b->denominator / divisor;
    unsigned long left, right, numerator, denominator;
    if (!checked_multiply(a->numerator, y, &left) || !checked_multiply(b->numerator, x, &right) ||
        !checked_add(left, right, &numerator) || !checked_multiply(a->denominator, y, &denominator)) {
        return STYLE_FRACTION_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return STYLE_FRACTION_OK;
}

StyleFractionResult style_fraction_multiply(const StyleFraction *a, const StyleFraction *b, StyleFraction *out) {
    StyleFractionResult status = validate(a);
    if (status == STYLE_FRACTION_OK) {
        status = validate(b);
    }
    if (status != STYLE_FRACTION_OK) {
        return status;
    }
    if (out == NULL) {
        return STYLE_FRACTION_ERR_NULL_ARGUMENT;
    }
    /* Cancel across first so the product overflows only if its reduced form does. */
    unsigned long cross_a = gcd(a->numerator, b->denominator);
    unsigned long cross_b = gcd(b->numerator, a->denominator);
    unsigned long numerator, denominator;
    if (!checked_multiply(a->numerator / cross_a, b->numerator / cross_b, &numerator) ||
        !checked_multiply(a->denominator / cross_b, b->denominator / cross_a, &denominator)) {
        return STYLE_FRACTION_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return STYLE_FRACTION_OK;
}
