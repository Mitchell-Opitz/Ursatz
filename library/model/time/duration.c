#include "model/time/duration.h"

#include <limits.h>
#include <stdlib.h>

DurationResult duration_make(long long num, long long den, Duration *out) {
    if (den <= 0) {
        return DURATION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    Rational r;
    if (rational_make(num, den, &r) != RATIONAL_OK) {
        return DURATION_ERR_OVERFLOW;
    }
    return duration_of(r, out);
}

DurationResult duration_of(Rational value, Duration *out) {
    if (rational_is_negative(value)) {
        return DURATION_ERR_NEGATIVE;
    }
    out->value = value;
    return DURATION_OK;
}

int duration_cmp(Duration a, Duration b) {
    return rational_cmp(a.value, b.value);
}

bool duration_same(Duration a, Duration b) {
    return rational_equals(a.value, b.value);
}

DurationResult duration_plus(Duration a, Duration b, Duration *out) {
    Rational sum;
    if (rational_add(a.value, b.value, &sum) != RATIONAL_OK) {
        return DURATION_ERR_OVERFLOW;
    }
    out->value = sum;
    return DURATION_OK;
}

DurationResult duration_create(long numerator, unsigned long denominator, Duration **out) {
    if (denominator == 0) {
        return DURATION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (numerator < 0) {
        return DURATION_ERR_NEGATIVE;
    }
    if (denominator > (unsigned long)LLONG_MAX) {
        return DURATION_ERR_OVERFLOW;
    }
    Rational r;
    if (rational_make(numerator, (long long)denominator, &r) != RATIONAL_OK) {
        return DURATION_ERR_OVERFLOW;
    }
    Duration *d = (Duration *)malloc(sizeof(Duration));
    if (d == NULL) {
        return DURATION_ERR_ALLOC;
    }
    d->value = r;
    *out = d;
    return DURATION_OK;
}

void duration_destroy(Duration *duration) {
    free(duration);
}

bool duration_equals(const Duration *a, const Duration *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return rational_equals(a->value, b->value);
}

long duration_numerator(const Duration *duration) {
    return (long)duration->value.num;
}

unsigned long duration_denominator(const Duration *duration) {
    return (unsigned long)duration->value.den;
}

DurationResult duration_add(const Duration *a, const Duration *b, Duration **out) {
    Duration sum;
    DurationResult r = duration_plus(*a, *b, &sum);
    if (r != DURATION_OK) {
        return r;
    }
    return duration_create((long)sum.value.num, (unsigned long)sum.value.den, out);
}
