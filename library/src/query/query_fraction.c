#include "query/query_fraction.h"

#include <limits.h>

#include "core/rational.h"

static QueryFraction reduced(unsigned long numerator, unsigned long denominator) {
    unsigned long divisor = rational_gcd_unsigned(numerator, denominator);
    QueryFraction result = {numerator / divisor, denominator / divisor};
    return result;
}

QueryFractionResult query_fraction_reduce(QueryFraction fraction, QueryFraction *out) {
    if (fraction.denominator == 0) {
        return QUERY_FRACTION_ERR_ZERO_DENOMINATOR;
    }
    *out = reduced(fraction.numerator, fraction.denominator);
    return QUERY_FRACTION_OK;
}

QueryFractionResult query_fraction_add(QueryFraction a, QueryFraction b, QueryFraction *out) {
    if (a.denominator == 0 || b.denominator == 0) {
        return QUERY_FRACTION_ERR_ZERO_DENOMINATOR;
    }
    /* Over the least common denominator: a/(g*x) + b/(g*y) = (a*y + b*x) / (g*x*y). */
    unsigned long divisor = rational_gcd_unsigned(a.denominator, b.denominator);
    unsigned long x = a.denominator / divisor;
    unsigned long y = b.denominator / divisor;
    unsigned long long left, right, numerator, denominator;
    if (!rational_checked_mul_unsigned(a.numerator, y, &left) || !rational_checked_mul_unsigned(b.numerator, x, &right) ||
        !rational_checked_add_unsigned(left, right, &numerator) || !rational_checked_mul_unsigned(a.denominator, y, &denominator)) {
        return QUERY_FRACTION_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return QUERY_FRACTION_OK;
}

QueryFractionResult query_fraction_multiply(QueryFraction a, QueryFraction b, QueryFraction *out) {
    if (a.denominator == 0 || b.denominator == 0) {
        return QUERY_FRACTION_ERR_ZERO_DENOMINATOR;
    }
    /* Cancel across first so the product overflows only if its reduced form does. */
    unsigned long cross_a = rational_gcd_unsigned(a.numerator, b.denominator);
    unsigned long cross_b = rational_gcd_unsigned(b.numerator, a.denominator);
    unsigned long long numerator, denominator;
    if (!rational_checked_mul_unsigned(a.numerator / cross_a, b.numerator / cross_b, &numerator) ||
        !rational_checked_mul_unsigned(a.denominator / cross_b, b.denominator / cross_a, &denominator)) {
        return QUERY_FRACTION_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return QUERY_FRACTION_OK;
}

QueryFractionResult query_fraction_compare(QueryFraction a, QueryFraction b, int *out_comparison) {
    if (a.denominator == 0 || b.denominator == 0) {
        return QUERY_FRACTION_ERR_ZERO_DENOMINATOR;
    }
    unsigned long n1 = a.numerator, d1 = a.denominator;
    unsigned long n2 = b.numerator, d2 = b.denominator;
    int sign = 1;
    for (;;) {
        unsigned long q1 = n1 / d1, q2 = n2 / d2;
        if (q1 != q2) {
            *out_comparison = (q1 < q2 ? -1 : 1) * sign;
            return QUERY_FRACTION_OK;
        }
        unsigned long r1 = n1 % d1, r2 = n2 % d2;
        if (r1 == 0 || r2 == 0) {
            int comparison = (r1 == 0 && r2 == 0) ? 0 : (r1 == 0 ? -1 : 1);
            *out_comparison = comparison * sign;
            return QUERY_FRACTION_OK;
        }
        /* Same whole part: compare the fractional parts r1/d1 and r2/d2 by their reciprocals, order reversed. */
        n1 = d1;
        d1 = r1;
        n2 = d2;
        d2 = r2;
        sign = -sign;
    }
}
