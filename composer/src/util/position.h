#ifndef URSATZ_COMPOSER_POSITION_H
#define URSATZ_COMPOSER_POSITION_H

#include <stdbool.h>

#include <core/rational.h>

/*
 * Positions and lengths inside an operator: library Rationals, in whole notes. The operators add, subtract and compare
 * them in search loops that have no place to report a failure, so these wrap the checked library arithmetic: a result
 * that would overflow is POSITION_FAR, a position far past any phrase. It stays far through further arithmetic (it is
 * never less than a real position), so a note placed there fails the length and chord checks and the candidate is
 * refused, which is the operator's failure. Private: not installed, not part of the API.
 */

#define POSITION_ZERO ((Rational){0, 1})
/* No real position is as far as this many whole notes. */
#define POSITION_FAR_LIMIT (1LL << 59)

static inline Rational position_far(void) {
    Rational far = {POSITION_FAR_LIMIT, 1};
    return far;
}

static inline bool position_is_far(Rational a) {
    Rational far = position_far();
    Rational behind = {-POSITION_FAR_LIMIT, 1};
    return rational_cmp(a, far) >= 0 || rational_cmp(a, behind) <= 0;
}

static inline Rational position_add(Rational a, Rational b) {
    Rational out;
    if (position_is_far(a) || position_is_far(b) || rational_add(a, b, &out) != RATIONAL_OK) {
        return position_far();
    }
    return out;
}

static inline Rational position_sub(Rational a, Rational b) {
    Rational out;
    if (position_is_far(a) || position_is_far(b) || rational_sub(a, b, &out) != RATIONAL_OK) {
        return position_far();
    }
    return out;
}

/* a times k. */
static inline Rational position_mul(Rational a, long long k) {
    Rational out;
    if (position_is_far(a) || rational_mul_int(a, k, &out) != RATIONAL_OK) {
        return position_far();
    }
    return out;
}

/* a divided by k, for k not zero. */
static inline Rational position_div(Rational a, long long k) {
    Rational out;
    if (position_is_far(a) || rational_div_int(a, k, &out) != RATIONAL_OK) {
        return position_far();
    }
    return out;
}

static inline bool position_is_positive(Rational a) {
    return a.num > 0;
}

/* The greatest whole number of `unit` (a positive length) at or before a: floor(a / unit). */
static inline long long position_floor_units(Rational a, Rational unit) {
    Rational ratio;
    long long whole = 0;
    if (!position_is_positive(unit) || rational_div(a, unit, &ratio) != RATIONAL_OK || rational_floor(ratio, &whole) != RATIONAL_OK) {
        return 0;
    }
    return whole;
}

/* Whether a is a whole number of `unit` (a positive length). */
static inline bool position_is_multiple(Rational a, Rational unit) {
    Rational ratio;
    return position_is_positive(unit) && rational_div(a, unit, &ratio) == RATIONAL_OK && ratio.den == 1;
}

/* a - floor(a / unit) * unit: where a falls inside its unit, 0 to unit. */
static inline Rational position_remainder(Rational a, Rational unit) {
    return position_sub(a, position_mul(unit, position_floor_units(a, unit)));
}

/* The greatest length of which both a and b (positive lengths) are whole multiples. */
static inline Rational position_gcd(Rational a, Rational b) {
    while (position_is_positive(b)) {
        Rational remainder = position_remainder(a, b);
        a = b;
        b = remainder;
    }
    return a;
}

/* k * part / whole cut toward zero to a whole number, for a positive whole: how far through `whole` a part is, in k ths. */
static inline long long position_scale(long long k, Rational part, Rational whole) {
    Rational scaled;
    if (!position_is_positive(whole) || rational_mul_int(part, k, &scaled) != RATIONAL_OK ||
        rational_div(scaled, whole, &scaled) != RATIONAL_OK) {
        return 0;
    }
    return scaled.num / scaled.den;
}

/* |a|. */
static inline Rational position_abs(Rational a) {
    return rational_is_negative(a) ? position_sub(POSITION_ZERO, a) : a;
}

#endif
