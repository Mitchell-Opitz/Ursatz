#include "time/duration.h"

#include <limits.h>
#include <stdlib.h>
#include "core/rational.h"
#include "core/value.h"

struct Duration {
    Value *value;
};

typedef struct {
    long numerator;
    unsigned long denominator;
} DurationContent;

DurationResult duration_create(long numerator, unsigned long denominator, Duration **out) {
    if (denominator == 0) {
        return DURATION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (numerator < 0) {
        return DURATION_ERR_NEGATIVE;
    }

    unsigned long divisor = (unsigned long)rational_gcd_unsigned((unsigned long)numerator, denominator);
    if (divisor == 0) {
        divisor = 1;
    }

    DurationContent content;
    content.numerator = numerator / (long)divisor;
    content.denominator = denominator / divisor;

    Duration *d = (Duration *)malloc(sizeof(Duration));
    if (d == NULL) {
        return DURATION_ERR_ALLOC;
    }

    if (value_create("Duration", &content, sizeof(content), &d->value) != VALUE_OK) {
        free(d);
        return DURATION_ERR_ALLOC;
    }

    *out = d;
    return DURATION_OK;
}

void duration_destroy(Duration *duration) {
    if (duration == NULL) {
        return;
    }
    value_destroy(duration->value);
    free(duration);
}

bool duration_equals(const Duration *a, const Duration *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}

static const DurationContent *duration_content(const Duration *duration) {
    return (const DurationContent *)value_content(duration->value, NULL);
}

long duration_numerator(const Duration *duration) {
    return duration_content(duration)->numerator;
}

unsigned long duration_denominator(const Duration *duration) {
    return duration_content(duration)->denominator;
}

static bool rational_of_content(const DurationContent *content, Rational *out) {
    return content->denominator <= (unsigned long)LLONG_MAX &&
           rational_make(content->numerator, (long long)content->denominator, out) == RATIONAL_OK;
}

static DurationResult create_from_rational(Rational value, Duration **out) {
    if (value.num < 0) {
        return DURATION_ERR_NEGATIVE;
    }
    if (value.num > LONG_MAX || (unsigned long long)value.den > ULONG_MAX) {
        return DURATION_ERR_OVERFLOW;
    }
    return duration_create((long)value.num, (unsigned long)value.den, out);
}

DurationResult duration_add(const Duration *a, const Duration *b, Duration **out) {
    Rational left;
    Rational right;
    Rational sum;
    if (!rational_of_content(duration_content(a), &left) || !rational_of_content(duration_content(b), &right) ||
        rational_add(left, right, &sum) != RATIONAL_OK) {
        return DURATION_ERR_OVERFLOW;
    }
    return create_from_rational(sum, out);
}

DurationResult duration_subtract(const Duration *a, const Duration *b, Duration **out) {
    Rational left;
    Rational right;
    Rational difference;
    if (!rational_of_content(duration_content(a), &left) || !rational_of_content(duration_content(b), &right) ||
        rational_sub(left, right, &difference) != RATIONAL_OK) {
        return DURATION_ERR_OVERFLOW;
    }
    return create_from_rational(difference, out);
}
