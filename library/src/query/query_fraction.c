#include "query/query_fraction.h"

#include <limits.h>

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

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

static QueryFraction reduced(unsigned long numerator, unsigned long denominator) {
    unsigned long divisor = gcd(numerator, denominator);
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
    unsigned long divisor = gcd(a.denominator, b.denominator);
    unsigned long x = a.denominator / divisor;
    unsigned long y = b.denominator / divisor;
    unsigned long left, right, numerator, denominator;
    if (!checked_multiply(a.numerator, y, &left) || !checked_multiply(b.numerator, x, &right) ||
        !checked_add(left, right, &numerator) || !checked_multiply(a.denominator, y, &denominator)) {
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
    unsigned long cross_a = gcd(a.numerator, b.denominator);
    unsigned long cross_b = gcd(b.numerator, a.denominator);
    unsigned long numerator, denominator;
    if (!checked_multiply(a.numerator / cross_a, b.numerator / cross_b, &numerator) ||
        !checked_multiply(a.denominator / cross_b, b.denominator / cross_a, &denominator)) {
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
