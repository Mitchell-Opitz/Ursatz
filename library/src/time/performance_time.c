#include "time/performance_time.h"

#include <limits.h>
#include <stdlib.h>

#include "core/value.h"

struct PerformanceTime {
    Value *value;
};

typedef struct {
    long numerator;
    unsigned long denominator;
} PerformanceTimeContent;

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* Plain-C checked arithmetic on unsigned long long: 0 on success, -1 on overflow. */
static int mul_checked(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (a != 0 && b > ULLONG_MAX / a) {
        return -1;
    }
    *out = a * b;
    return 0;
}

static int add_checked(unsigned long long a, unsigned long long b, unsigned long long *out) {
    if (b > ULLONG_MAX - a) {
        return -1;
    }
    *out = a + b;
    return 0;
}

static PerformanceTimeResult build(unsigned long long numerator, unsigned long long denominator,
                                    PerformanceTime **out) {
    unsigned long long divisor = gcd_ull(numerator, denominator);
    if (divisor > 1) {
        numerator /= divisor;
        denominator /= divisor;
    }
    if (numerator > (unsigned long long)LONG_MAX || denominator > (unsigned long long)ULONG_MAX) {
        return PERFORMANCE_TIME_ERR_OVERFLOW;
    }

    PerformanceTimeContent content;
    content.numerator = (long)numerator;
    content.denominator = (unsigned long)denominator;

    PerformanceTime *t = (PerformanceTime *)malloc(sizeof(PerformanceTime));
    if (t == NULL) {
        return PERFORMANCE_TIME_ERR_ALLOC;
    }
    if (value_create("PerformanceTime", &content, sizeof(content), &t->value) != VALUE_OK) {
        free(t);
        return PERFORMANCE_TIME_ERR_ALLOC;
    }
    *out = t;
    return PERFORMANCE_TIME_OK;
}

PerformanceTimeResult performance_time_create(long numerator, unsigned long denominator, PerformanceTime **out) {
    if (out == NULL) {
        return PERFORMANCE_TIME_ERR_NULL_ARGUMENT;
    }
    if (denominator == 0) {
        return PERFORMANCE_TIME_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (numerator < 0) {
        return PERFORMANCE_TIME_ERR_NEGATIVE;
    }
    return build((unsigned long long)numerator, (unsigned long long)denominator, out);
}

void performance_time_destroy(PerformanceTime *time) {
    if (time == NULL) {
        return;
    }
    value_destroy(time->value);
    free(time);
}

static const PerformanceTimeContent *content_of(const PerformanceTime *time) {
    return (const PerformanceTimeContent *)value_content(time->value, NULL);
}

long performance_time_numerator(const PerformanceTime *time) {
    return content_of(time)->numerator;
}

unsigned long performance_time_denominator(const PerformanceTime *time) {
    return content_of(time)->denominator;
}

bool performance_time_equals(const PerformanceTime *a, const PerformanceTime *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}

/* Compares a/b with c/d without multiplying: compare the integer parts, then the reciprocals of
   the remainders (which swaps the order), repeating as in Euclid's algorithm. */
int performance_time_compare(const PerformanceTime *x, const PerformanceTime *y) {
    unsigned long long a = (unsigned long long)content_of(x)->numerator;
    unsigned long long b = content_of(x)->denominator;
    unsigned long long c = (unsigned long long)content_of(y)->numerator;
    unsigned long long d = content_of(y)->denominator;
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
        /* a/b vs c/d with equal integer parts is ra/b vs rc/d, the reverse of b/ra vs d/rc. */
        a = b;
        b = ra;
        c = d;
        d = rc;
        sign = -sign;
    }
}

PerformanceTimeResult performance_time_add(const PerformanceTime *a, const PerformanceTime *b,
                                            PerformanceTime **out) {
    if (a == NULL || b == NULL || out == NULL) {
        return PERFORMANCE_TIME_ERR_NULL_ARGUMENT;
    }
    unsigned long long an = (unsigned long long)content_of(a)->numerator;
    unsigned long long ad = content_of(a)->denominator;
    unsigned long long bn = (unsigned long long)content_of(b)->numerator;
    unsigned long long bd = content_of(b)->denominator;

    /* Over the least common denominator, so the products stay as small as they can. */
    unsigned long long g = gcd_ull(ad, bd);
    unsigned long long ad_over_g = ad / g;
    unsigned long long bd_over_g = bd / g;
    unsigned long long left = 0;
    unsigned long long right = 0;
    unsigned long long numerator = 0;
    unsigned long long denominator = 0;
    if (mul_checked(an, bd_over_g, &left) != 0 || mul_checked(bn, ad_over_g, &right) != 0 ||
        add_checked(left, right, &numerator) != 0 || mul_checked(ad_over_g, bd, &denominator) != 0) {
        return PERFORMANCE_TIME_ERR_OVERFLOW;
    }
    return build(numerator, denominator, out);
}
