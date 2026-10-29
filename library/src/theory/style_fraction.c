#include "theory/style_fraction.h"

#include <limits.h>
#include <stddef.h>

#include "core/rational.h"

static StyleFractionResult validate(const StyleFraction *fraction) {
    if (fraction == NULL) {
        return STYLE_FRACTION_ERR_NULL_ARGUMENT;
    }
    return fraction->denominator == 0 ? STYLE_FRACTION_ERR_ZERO_DENOMINATOR : STYLE_FRACTION_OK;
}

static StyleFraction reduced(unsigned long numerator, unsigned long denominator) {
    unsigned long divisor = rational_gcd_unsigned(numerator, denominator);
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
    unsigned long divisor = rational_gcd_unsigned(a->denominator, b->denominator);
    unsigned long x = a->denominator / divisor;
    unsigned long y = b->denominator / divisor;
    unsigned long long left, right, numerator, denominator;
    if (!rational_checked_mul_unsigned(a->numerator, y, &left) || !rational_checked_mul_unsigned(b->numerator, x, &right) ||
        !rational_checked_add_unsigned(left, right, &numerator) || !rational_checked_mul_unsigned(a->denominator, y, &denominator)) {
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
    unsigned long cross_a = rational_gcd_unsigned(a->numerator, b->denominator);
    unsigned long cross_b = rational_gcd_unsigned(b->numerator, a->denominator);
    unsigned long long numerator, denominator;
    if (!rational_checked_mul_unsigned(a->numerator / cross_a, b->numerator / cross_b, &numerator) ||
        !rational_checked_mul_unsigned(a->denominator / cross_b, b->denominator / cross_a, &denominator)) {
        return STYLE_FRACTION_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return STYLE_FRACTION_OK;
}
