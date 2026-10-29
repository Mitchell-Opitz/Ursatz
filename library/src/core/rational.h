#ifndef MUSIC_CORE_RATIONAL_H
#define MUSIC_CORE_RATIONAL_H

#include <stdbool.h>

/*
 * Rational: a signed exact rational on 64-bit integers, always in lowest terms with den > 0. Zero is 0/1.
 * Every operation that can overflow is checked: it returns an error and leaves *out untouched, never
 * wraps. A plain value type, no allocation. LLONG_MIN is never a valid numerator or denominator.
 *
 * rational_cmp never multiplies (Euclid's method on the integer parts and remainders), so it cannot
 * overflow. Callers on non-negative MusicalTime or Duration values use the bridges in time/time_rational.h.
 */

typedef struct {
    long long num;
    long long den;
} Rational;

typedef enum {
    RATIONAL_OK = 0,
    RATIONAL_ERR_NULL,
    RATIONAL_ERR_ZERO_DENOMINATOR,
    RATIONAL_ERR_OVERFLOW
} RationalResult;

/* num/den normalized: sign moved to the numerator, reduced. LLONG_MIN in either part is overflow. */
RationalResult rational_make(long long num, long long den, Rational *out);
RationalResult rational_from_int(long long value, Rational *out);
/* num/den for callers whose arguments are known valid (den > 0, no LLONG_MIN), such as constants and
   values already read from a checked source. An invalid pair yields 0/1; use rational_make when the
   inputs are not known. */
Rational rational_ratio(long long num, long long den);

RationalResult rational_add(Rational a, Rational b, Rational *out);
RationalResult rational_sub(Rational a, Rational b, Rational *out);
RationalResult rational_mul(Rational a, Rational b, Rational *out);
/* A zero divisor is RATIONAL_ERR_ZERO_DENOMINATOR. */
RationalResult rational_div(Rational a, Rational b, Rational *out);
RationalResult rational_neg(Rational a, Rational *out);
RationalResult rational_mul_int(Rational a, long long k, Rational *out);
/* k == 0 is RATIONAL_ERR_ZERO_DENOMINATOR. */
RationalResult rational_div_int(Rational a, long long k, Rational *out);

/* Largest integer <= a, smallest integer >= a. Both always fit. */
RationalResult rational_floor(Rational a, long long *out);
RationalResult rational_ceil(Rational a, long long *out);

/* -1, 0 or 1. Overflow-free. */
int rational_cmp(Rational a, Rational b);
bool rational_equals(Rational a, Rational b);
bool rational_is_zero(Rational a);
bool rational_is_negative(Rational a);

/* Greatest common divisor, never negative. gcd(0, 0) is 0. A result of 2^63 (only for LLONG_MIN
   inputs) saturates to LLONG_MAX. */
long long rational_gcd(long long a, long long b);

/* For callers whose stored fields are unsigned and may exceed LLONG_MAX (MusicalTime, Duration and
   PerformanceTime denominators). The gcd is exact; the comparison is of two non-negative fractions
   an/ad and bn/bd (ad, bd > 0), overflow-free like rational_cmp. */
unsigned long long rational_gcd_unsigned(unsigned long long a, unsigned long long b);
int rational_cmp_unsigned(unsigned long long an, unsigned long long ad, unsigned long long bn, unsigned long long bd);

/* Checked unsigned steps for the fraction types whose fields use the full unsigned range. Return false
   on overflow and leave *out untouched. */
bool rational_checked_mul_unsigned(unsigned long long a, unsigned long long b, unsigned long long *out);
bool rational_checked_add_unsigned(unsigned long long a, unsigned long long b, unsigned long long *out);

#endif
