#include "time/performance_time.h"

#include <limits.h>
#include <stdlib.h>

#include "core/rational.h"
#include "core/value.h"

struct PerformanceTime {
    Value *value;
};

typedef struct {
    long numerator;
    unsigned long denominator;
} PerformanceTimeContent;

static PerformanceTimeResult build(unsigned long long numerator, unsigned long long denominator,
                                    PerformanceTime **out) {
    unsigned long long divisor = rational_gcd_unsigned(numerator, denominator);
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

int performance_time_compare(const PerformanceTime *x, const PerformanceTime *y) {
    return rational_cmp_unsigned((unsigned long long)content_of(x)->numerator, content_of(x)->denominator,
                                 (unsigned long long)content_of(y)->numerator, content_of(y)->denominator);
}

PerformanceTimeResult performance_time_add(const PerformanceTime *a, const PerformanceTime *b,
                                            PerformanceTime **out) {
    if (a == NULL || b == NULL || out == NULL) {
        return PERFORMANCE_TIME_ERR_NULL_ARGUMENT;
    }
    /* Rational holds at most LLONG_MAX in a field: a larger stored denominator is reported as overflow. */
    Rational left;
    Rational right;
    Rational sum;
    if (content_of(a)->denominator > (unsigned long)LLONG_MAX || content_of(b)->denominator > (unsigned long)LLONG_MAX ||
        rational_make(content_of(a)->numerator, (long long)content_of(a)->denominator, &left) != RATIONAL_OK ||
        rational_make(content_of(b)->numerator, (long long)content_of(b)->denominator, &right) != RATIONAL_OK ||
        rational_add(left, right, &sum) != RATIONAL_OK) {
        return PERFORMANCE_TIME_ERR_OVERFLOW;
    }
    return build((unsigned long long)sum.num, (unsigned long long)sum.den, out);
}
