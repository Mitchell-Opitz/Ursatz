#include "rules/preference_weight.h"

#include <limits.h>

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}

/* False (and *out untouched) when a * b does not fit. */
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

static PreferenceWeightResult validate(const PreferenceFraction *fraction) {
    if (fraction == NULL) {
        return PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT;
    }
    if (fraction->denominator == 0) {
        return PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR;
    }
    return PREFERENCE_WEIGHT_OK;
}

static PreferenceFraction reduced(unsigned long numerator, unsigned long denominator) {
    unsigned long divisor = gcd(numerator, denominator);
    PreferenceFraction result = {numerator / divisor, denominator / divisor};
    return result;
}

PreferenceWeightResult preference_fraction_reduce(const PreferenceFraction *fraction, PreferenceFraction *out) {
    PreferenceWeightResult status = validate(fraction);
    if (status != PREFERENCE_WEIGHT_OK) {
        return status;
    }
    if (out == NULL) {
        return PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT;
    }
    *out = reduced(fraction->numerator, fraction->denominator);
    return PREFERENCE_WEIGHT_OK;
}

PreferenceWeightResult preference_fraction_add(const PreferenceFraction *a, const PreferenceFraction *b,
                                               PreferenceFraction *out) {
    PreferenceWeightResult status = validate(a);
    if (status == PREFERENCE_WEIGHT_OK) {
        status = validate(b);
    }
    if (status != PREFERENCE_WEIGHT_OK) {
        return status;
    }
    if (out == NULL) {
        return PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT;
    }

    /* Over the least common denominator: a/(g*x) + b/(g*y) = (a*y + b*x) / (g*x*y). */
    unsigned long divisor = gcd(a->denominator, b->denominator);
    unsigned long x = a->denominator / divisor;
    unsigned long y = b->denominator / divisor;
    unsigned long left, right, numerator, denominator;
    if (!checked_multiply(a->numerator, y, &left) || !checked_multiply(b->numerator, x, &right) ||
        !checked_add(left, right, &numerator) || !checked_multiply(a->denominator, y, &denominator)) {
        return PREFERENCE_WEIGHT_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return PREFERENCE_WEIGHT_OK;
}

PreferenceWeightResult preference_fraction_multiply(const PreferenceFraction *a, const PreferenceFraction *b,
                                                    PreferenceFraction *out) {
    PreferenceWeightResult status = validate(a);
    if (status == PREFERENCE_WEIGHT_OK) {
        status = validate(b);
    }
    if (status != PREFERENCE_WEIGHT_OK) {
        return status;
    }
    if (out == NULL) {
        return PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT;
    }

    /* Cancel across first so the product overflows only if its reduced form does. */
    unsigned long cross_a = gcd(a->numerator, b->denominator);
    unsigned long cross_b = gcd(b->numerator, a->denominator);
    unsigned long numerator, denominator;
    if (!checked_multiply(a->numerator / cross_a, b->numerator / cross_b, &numerator) ||
        !checked_multiply(a->denominator / cross_b, b->denominator / cross_a, &denominator)) {
        return PREFERENCE_WEIGHT_ERR_OVERFLOW;
    }
    *out = reduced(numerator, denominator);
    return PREFERENCE_WEIGHT_OK;
}

PreferenceWeightResult preference_fraction_compare(const PreferenceFraction *a, const PreferenceFraction *b,
                                                   int *out_comparison) {
    PreferenceWeightResult status = validate(a);
    if (status == PREFERENCE_WEIGHT_OK) {
        status = validate(b);
    }
    if (status != PREFERENCE_WEIGHT_OK) {
        return status;
    }
    if (out_comparison == NULL) {
        return PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT;
    }

    unsigned long n1 = a->numerator, d1 = a->denominator;
    unsigned long n2 = b->numerator, d2 = b->denominator;
    int sign = 1;
    for (;;) {
        unsigned long q1 = n1 / d1, q2 = n2 / d2;
        if (q1 != q2) {
            *out_comparison = (q1 < q2 ? -1 : 1) * sign;
            return PREFERENCE_WEIGHT_OK;
        }
        unsigned long r1 = n1 % d1, r2 = n2 % d2;
        if (r1 == 0 || r2 == 0) {
            int comparison = (r1 == 0 && r2 == 0) ? 0 : (r1 == 0 ? -1 : 1);
            *out_comparison = comparison * sign;
            return PREFERENCE_WEIGHT_OK;
        }
        /* Same whole part: compare the fractional parts r1/d1 and r2/d2 by their reciprocals, order reversed. */
        n1 = d1;
        d1 = r1;
        n2 = d2;
        d2 = r2;
        sign = -sign;
    }
}

PreferenceWeightResult preference_fraction_weighted_total(const PreferenceFraction *scores,
                                                          const PreferenceFraction *weights, size_t count,
                                                          PreferenceFraction *out) {
    if (out == NULL || (count > 0 && (scores == NULL || weights == NULL))) {
        return PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT;
    }

    PreferenceFraction weighted_sum = {0, 1};
    PreferenceFraction weight_sum = {0, 1};
    for (size_t i = 0; i < count; i++) {
        PreferenceWeightResult status = validate(&scores[i]);
        if (status == PREFERENCE_WEIGHT_OK) {
            status = validate(&weights[i]);
        }
        PreferenceFraction term;
        if (status == PREFERENCE_WEIGHT_OK) {
            status = preference_fraction_multiply(&weights[i], &scores[i], &term);
        }
        if (status == PREFERENCE_WEIGHT_OK) {
            status = preference_fraction_add(&weighted_sum, &term, &weighted_sum);
        }
        if (status == PREFERENCE_WEIGHT_OK) {
            status = preference_fraction_add(&weight_sum, &weights[i], &weight_sum);
        }
        if (status != PREFERENCE_WEIGHT_OK) {
            return status;
        }
    }

    if (weight_sum.numerator == 0) {
        out->numerator = 0;
        out->denominator = 1;
        return PREFERENCE_WEIGHT_OK;
    }
    /* Dividing by weight_sum is multiplying by its reciprocal. */
    PreferenceFraction reciprocal = {weight_sum.denominator, weight_sum.numerator};
    return preference_fraction_multiply(&weighted_sum, &reciprocal, out);
}
