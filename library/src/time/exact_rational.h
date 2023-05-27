#ifndef MUSIC_TIME_EXACT_RATIONAL_H
#define MUSIC_TIME_EXACT_RATIONAL_H

#include <limits.h>
#include <stdbool.h>

#include "time/duration.h"
#include "time/musical_time.h"

/*
 * ExactRational: an internal helper for code that does arithmetic on MusicalTime and Duration
 * positions without risking a silent wrap. A non-negative rational on unsigned 64-bit integers, always
 * in lowest terms, with every operation checked: it returns -1 on overflow and writes nothing. It is
 * not a public value type (MusicalTime, Duration and PerformanceTime are), and it holds no meaning of
 * its own: it is only the arithmetic. Header-only (static inline), so each user compiles its own copy.
 *
 * All values are non-negative. er_sub requires a >= b.
 */

typedef struct {
    unsigned long long n;
    unsigned long long d;
} ExactRational;

static inline unsigned long long er_gcd(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static inline int er_mul_u64(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ULLONG_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static inline int er_add_u64(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (b > ULLONG_MAX - a) {
        return -1;
    }
    *out = a + b;
    return 0;
}

/* n / d in lowest terms; d must be non-zero. */
static inline ExactRational er_make(unsigned long long n, unsigned long long d) {
    unsigned long long g = er_gcd(n, d);
    ExactRational r;
    r.n = g > 1 ? n / g : n;
    r.d = g > 1 ? d / g : d;
    return r;
}

/* The value of a MusicalTime or Duration. The time must not be negative. */
static inline ExactRational er_of_time(const MusicalTime *time) {
    return er_make((unsigned long long)musical_time_numerator(time), musical_time_denominator(time));
}

static inline ExactRational er_of_duration(const Duration *duration) {
    return er_make((unsigned long long)duration_numerator(duration), duration_denominator(duration));
}

static inline int er_add(ExactRational a, ExactRational b, ExactRational *out) {
    unsigned long long g = er_gcd(a.d, b.d);
    unsigned long long ad_over_g = a.d / g;
    unsigned long long bd_over_g = b.d / g;
    unsigned long long left = 0;
    unsigned long long right = 0;
    unsigned long long n = 0;
    unsigned long long d = 0;
    if (er_mul_u64(a.n, bd_over_g, &left) != 0 || er_mul_u64(b.n, ad_over_g, &right) != 0 ||
        er_add_u64(left, right, &n) != 0 || er_mul_u64(ad_over_g, b.d, &d) != 0) {
        return -1;
    }
    *out = er_make(n, d);
    return 0;
}

/* a - b, for a >= b. */
static inline int er_sub(ExactRational a, ExactRational b, ExactRational *out) {
    unsigned long long g = er_gcd(a.d, b.d);
    unsigned long long ad_over_g = a.d / g;
    unsigned long long bd_over_g = b.d / g;
    unsigned long long left = 0;
    unsigned long long right = 0;
    unsigned long long d = 0;
    if (er_mul_u64(a.n, bd_over_g, &left) != 0 || er_mul_u64(b.n, ad_over_g, &right) != 0 ||
        er_mul_u64(ad_over_g, b.d, &d) != 0) {
        return -1;
    }
    *out = er_make(left - right, d);
    return 0;
}

static inline int er_mul(ExactRational a, ExactRational b, ExactRational *out) {
    unsigned long long g1 = er_gcd(a.n, b.d);
    unsigned long long g2 = er_gcd(b.n, a.d);
    unsigned long long an = g1 > 1 ? a.n / g1 : a.n;
    unsigned long long bd = g1 > 1 ? b.d / g1 : b.d;
    unsigned long long bn = g2 > 1 ? b.n / g2 : b.n;
    unsigned long long ad = g2 > 1 ? a.d / g2 : a.d;
    unsigned long long n = 0;
    unsigned long long d = 0;
    if (er_mul_u64(an, bn, &n) != 0 || er_mul_u64(ad, bd, &d) != 0) {
        return -1;
    }
    *out = er_make(n, d);
    return 0;
}

/* -1, 0 or 1. Compares integer parts and then reciprocals of remainders, as in Euclid's algorithm, so
   it never multiplies and cannot overflow. */
static inline int er_cmp(ExactRational x, ExactRational y) {
    unsigned long long a = x.n;
    unsigned long long b = x.d;
    unsigned long long c = y.n;
    unsigned long long d = y.d;
    int sign = 1;
    for (;;) {
        unsigned long long qa = a / b;
        unsigned long long qc = c / d;
        if (qa != qc) {
            return qa < qc ? -sign : sign;
        }
        unsigned long long ra = a % b;
        unsigned long long rc = c % d;
        if (ra == 0 && rc == 0) {
            return 0;
        }
        if (ra == 0) {
            return -sign;
        }
        if (rc == 0) {
            return sign;
        }
        a = b;
        b = ra;
        c = d;
        d = rc;
        sign = -sign;
    }
}

/* A new MusicalTime with this value, or NULL if it does not fit a MusicalTime or on allocation
   failure. */
static inline MusicalTime *er_to_time(ExactRational r) {
    MusicalTime *t = NULL;
    if (r.n > (unsigned long long)LONG_MAX || r.d > (unsigned long long)ULONG_MAX ||
        musical_time_create((long)r.n, (unsigned long)r.d, &t) != MUSICAL_TIME_OK) {
        return NULL;
    }
    return t;
}

#endif
