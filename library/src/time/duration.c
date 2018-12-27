#include "time/duration.h"

#include <stdlib.h>
#include "core/value.h"

struct Duration {
    Value *value;
};

typedef struct {
    long numerator;
    unsigned long denominator;
} DurationContent;

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

DurationResult duration_create(long numerator, unsigned long denominator, Duration **out) {
    if (denominator == 0) {
        return DURATION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (numerator < 0) {
        return DURATION_ERR_NEGATIVE;
    }

    unsigned long divisor = gcd((unsigned long)numerator, denominator);
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

DurationResult duration_add(const Duration *a, const Duration *b, Duration **out) {
    const DurationContent *ca = duration_content(a);
    const DurationContent *cb = duration_content(b);

    long numerator = ca->numerator * (long)cb->denominator + cb->numerator * (long)ca->denominator;
    unsigned long denominator = ca->denominator * cb->denominator;

    return duration_create(numerator, denominator, out);
}

DurationResult duration_subtract(const Duration *a, const Duration *b, Duration **out) {
    const DurationContent *ca = duration_content(a);
    const DurationContent *cb = duration_content(b);

    long numerator = ca->numerator * (long)cb->denominator - cb->numerator * (long)ca->denominator;
    unsigned long denominator = ca->denominator * cb->denominator;

    if (numerator < 0) {
        return DURATION_ERR_NEGATIVE;
    }

    return duration_create(numerator, denominator, out);
}