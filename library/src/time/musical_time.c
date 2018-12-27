#include "time/musical_time.h"

#include <stdlib.h>
#include "core/value.h"

struct MusicalTime {
    Value *value;
};

typedef struct {
    long numerator;
    unsigned long denominator;
} MusicalTimeContent;

static unsigned long abs_ul(long n) {
    return n < 0 ? (unsigned long)(-n) : (unsigned long)n;
}

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

MusicalTimeResult musical_time_create(long numerator, unsigned long denominator, MusicalTime **out) {
    if (denominator == 0) {
        return MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR;
    }

    unsigned long divisor = gcd(abs_ul(numerator), denominator);
    if (divisor == 0) {
        divisor = 1;
    }

    MusicalTimeContent content;
    content.numerator = numerator / (long)divisor;
    content.denominator = denominator / divisor;

    MusicalTime *t = (MusicalTime *)malloc(sizeof(MusicalTime));
    if (t == NULL) {
        return MUSICAL_TIME_ERR_ALLOC;
    }

    if (value_create("MusicalTime", &content, sizeof(content), &t->value) != VALUE_OK) {
        free(t);
        return MUSICAL_TIME_ERR_ALLOC;
    }

    *out = t;
    return MUSICAL_TIME_OK;
}

void musical_time_destroy(MusicalTime *time) {
    if (time == NULL) {
        return;
    }
    value_destroy(time->value);
    free(time);
}

bool musical_time_equals(const MusicalTime *a, const MusicalTime *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}

static const MusicalTimeContent *musical_time_content(const MusicalTime *time) {
    return (const MusicalTimeContent *)value_content(time->value, NULL);
}

long musical_time_numerator(const MusicalTime *time) {
    return musical_time_content(time)->numerator;
}

unsigned long musical_time_denominator(const MusicalTime *time) {
    return musical_time_content(time)->denominator;
}

MusicalTimeResult musical_time_add(const MusicalTime *time, const Duration *duration, MusicalTime **out) {
    const MusicalTimeContent *ct = musical_time_content(time);
    long d_numerator = duration_numerator(duration);
    unsigned long d_denominator = duration_denominator(duration);

    long numerator = ct->numerator * (long)d_denominator + d_numerator * (long)ct->denominator;
    unsigned long denominator = ct->denominator * d_denominator;

    return musical_time_create(numerator, denominator, out);
}

DurationResult musical_time_subtract(const MusicalTime *a, const MusicalTime *b, Duration **out) {
    const MusicalTimeContent *ca = musical_time_content(a);
    const MusicalTimeContent *cb = musical_time_content(b);

    long numerator = ca->numerator * (long)cb->denominator - cb->numerator * (long)ca->denominator;
    unsigned long denominator = ca->denominator * cb->denominator;

    if (numerator < 0) {
        return DURATION_ERR_NEGATIVE;
    }

    return duration_create(numerator, denominator, out);
}