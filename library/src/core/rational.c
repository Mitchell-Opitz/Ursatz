#include "core/rational.h"

#include <limits.h>
#include <stddef.h>

static unsigned long long magnitude(long long v) {
    return v < 0 ? 0ULL - (unsigned long long)v : (unsigned long long)v;
}

unsigned long long rational_gcd_unsigned(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

long long rational_gcd(long long a, long long b) {
    unsigned long long g = rational_gcd_unsigned(magnitude(a), magnitude(b));
    return g > (unsigned long long)LLONG_MAX ? LLONG_MAX : (long long)g;
}

static bool mul_ll(long long a, long long b, long long *out) {
#ifdef __SIZEOF_INT128__
    __int128 product = (__int128)a * (__int128)b;
    if (product > LLONG_MAX || product < LLONG_MIN) {
        return false;
    }
    *out = (long long)product;
    return true;
#else
    if (a == 0 || b == 0) {
        *out = 0;
        return true;
    }
    if (a > 0 ? (b > 0 ? a > LLONG_MAX / b : b < LLONG_MIN / a) : (b > 0 ? a < LLONG_MIN / b : a < LLONG_MAX / b)) {
        return false;
    }
    *out = a * b;
    return true;
#endif
}

static bool add_ll(long long a, long long b, long long *out) {
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN + 1 - b)) {
        return false;
    }
    *out = a + b;
    return true;
}

RationalResult rational_make(long long num, long long den, Rational *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    if (den == 0) {
        return RATIONAL_ERR_ZERO_DENOMINATOR;
    }
    if (num == LLONG_MIN || den == LLONG_MIN) {
        return RATIONAL_ERR_OVERFLOW;
    }
    if (den < 0) {
        num = -num;
        den = -den;
    }
    long long g = rational_gcd(num, den);
    out->num = num / g;
    out->den = den / g;
    return RATIONAL_OK;
}

Rational rational_ratio(long long num, long long den) {
    Rational r = {0, 1};
    rational_make(num, den, &r);
    return r;
}

RationalResult rational_from_int(long long value, Rational *out) {
    return rational_make(value, 1, out);
}

RationalResult rational_add(Rational a, Rational b, Rational *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    long long g = rational_gcd(a.den, b.den);
    long long ad = a.den / g;
    long long bd = b.den / g;
    long long left = 0;
    long long right = 0;
    long long n = 0;
    long long d = 0;
    if (!mul_ll(a.num, bd, &left) || !mul_ll(b.num, ad, &right) || !add_ll(left, right, &n) ||
        !mul_ll(ad, b.den, &d)) {
        return RATIONAL_ERR_OVERFLOW;
    }
    return rational_make(n, d, out);
}

RationalResult rational_neg(Rational a, Rational *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    out->num = -a.num;
    out->den = a.den;
    return RATIONAL_OK;
}

RationalResult rational_sub(Rational a, Rational b, Rational *out) {
    Rational nb;
    if (rational_neg(b, &nb) != RATIONAL_OK) {
        return RATIONAL_ERR_NULL;
    }
    return rational_add(a, nb, out);
}

RationalResult rational_mul(Rational a, Rational b, Rational *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    long long g1 = rational_gcd(a.num, b.den);
    long long g2 = rational_gcd(b.num, a.den);
    if (g1 == 0) {
        g1 = 1;
    }
    if (g2 == 0) {
        g2 = 1;
    }
    long long n = 0;
    long long d = 0;
    if (!mul_ll(a.num / g1, b.num / g2, &n) || !mul_ll(a.den / g2, b.den / g1, &d)) {
        return RATIONAL_ERR_OVERFLOW;
    }
    return rational_make(n, d, out);
}

RationalResult rational_div(Rational a, Rational b, Rational *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    if (b.num == 0) {
        return RATIONAL_ERR_ZERO_DENOMINATOR;
    }
    Rational inverse;
    inverse.num = b.num < 0 ? -b.den : b.den;
    inverse.den = b.num < 0 ? -b.num : b.num;
    return rational_mul(a, inverse, out);
}

RationalResult rational_mul_int(Rational a, long long k, Rational *out) {
    Rational r;
    RationalResult status = rational_from_int(k, &r);
    return status != RATIONAL_OK ? status : rational_mul(a, r, out);
}

RationalResult rational_div_int(Rational a, long long k, Rational *out) {
    Rational r;
    RationalResult status = rational_from_int(k, &r);
    return status != RATIONAL_OK ? status : rational_div(a, r, out);
}

RationalResult rational_floor(Rational a, long long *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    long long q = a.num / a.den;
    *out = (a.num % a.den != 0 && a.num < 0) ? q - 1 : q;
    return RATIONAL_OK;
}

RationalResult rational_ceil(Rational a, long long *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    long long q = a.num / a.den;
    *out = (a.num % a.den != 0 && a.num > 0) ? q + 1 : q;
    return RATIONAL_OK;
}

static int sign_of(Rational a) {
    return a.num < 0 ? -1 : (a.num > 0 ? 1 : 0);
}

int rational_cmp_unsigned(unsigned long long an, unsigned long long ad, unsigned long long bn, unsigned long long bd) {
    unsigned long long x = an;
    unsigned long long y = ad;
    unsigned long long z = bn;
    unsigned long long w = bd;
    int sign = 1;
    for (;;) {
        unsigned long long qa = x / y;
        unsigned long long qc = z / w;
        if (qa != qc) {
            return qa < qc ? -sign : sign;
        }
        unsigned long long ra = x % y;
        unsigned long long rc = z % w;
        if (ra == 0 && rc == 0) {
            return 0;
        }
        if (ra == 0) {
            return -sign;
        }
        if (rc == 0) {
            return sign;
        }
        x = y;
        y = ra;
        z = w;
        w = rc;
        sign = -sign;
    }
}

int rational_cmp(Rational a, Rational b) {
    int sa = sign_of(a);
    int sb = sign_of(b);
    if (sa != sb) {
        return sa < sb ? -1 : 1;
    }
    if (sa == 0) {
        return 0;
    }
    int result = rational_cmp_unsigned(magnitude(a.num), (unsigned long long)a.den, magnitude(b.num),
                                       (unsigned long long)b.den);
    return sa < 0 ? -result : result;
}

bool rational_equals(Rational a, Rational b) {
    return a.num == b.num && a.den == b.den;
}

bool rational_is_zero(Rational a) {
    return a.num == 0;
}

bool rational_is_negative(Rational a) {
    return a.num < 0;
}

bool rational_checked_mul_unsigned(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ULLONG_MAX / a) {
        return false;
    }
    *out = a * b;
    return true;
}

bool rational_checked_add_unsigned(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (b > ULLONG_MAX - a) {
        return false;
    }
    *out = a + b;
    return true;
}

double rational_to_double(Rational a) {
    return (double)a.num / (double)a.den;
}
