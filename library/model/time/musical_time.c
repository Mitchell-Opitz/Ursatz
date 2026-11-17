#include "model/time/musical_time.h"

#include <limits.h>
#include <stdlib.h>

MusicalTimeResult musical_time_make(long long num, long long den, MusicalTime *out) {
    if (den <= 0) {
        return MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR;
    }
    Rational r;
    if (rational_make(num, den, &r) != RATIONAL_OK) {
        return MUSICAL_TIME_ERR_OVERFLOW;
    }
    out->value = r;
    return MUSICAL_TIME_OK;
}

int musical_time_cmp(MusicalTime a, MusicalTime b) {
    return rational_cmp(a.value, b.value);
}

bool musical_time_same(MusicalTime a, MusicalTime b) {
    return rational_equals(a.value, b.value);
}

MusicalTimeResult musical_time_plus(MusicalTime t, Duration d, MusicalTime *out) {
    Rational sum;
    if (rational_add(t.value, d.value, &sum) != RATIONAL_OK) {
        return MUSICAL_TIME_ERR_OVERFLOW;
    }
    out->value = sum;
    return MUSICAL_TIME_OK;
}

MusicalTimeResult musical_time_create(long numerator, unsigned long denominator, MusicalTime **out) {
    if (denominator == 0) {
        return MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (denominator > (unsigned long)LLONG_MAX) {
        return MUSICAL_TIME_ERR_OVERFLOW;
    }
    Rational r;
    if (rational_make(numerator, (long long)denominator, &r) != RATIONAL_OK) {
        return MUSICAL_TIME_ERR_OVERFLOW;
    }
    MusicalTime *t = (MusicalTime *)malloc(sizeof(MusicalTime));
    if (t == NULL) {
        return MUSICAL_TIME_ERR_ALLOC;
    }
    t->value = r;
    *out = t;
    return MUSICAL_TIME_OK;
}

void musical_time_destroy(MusicalTime *time) {
    free(time);
}

bool musical_time_equals(const MusicalTime *a, const MusicalTime *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return rational_equals(a->value, b->value);
}

long musical_time_numerator(const MusicalTime *time) {
    return (long)time->value.num;
}

unsigned long musical_time_denominator(const MusicalTime *time) {
    return (unsigned long)time->value.den;
}

MusicalTimeResult musical_time_add(const MusicalTime *time, const Duration *duration, MusicalTime **out) {
    MusicalTime sum;
    if (musical_time_plus(*time, *duration, &sum) != MUSICAL_TIME_OK) {
        return MUSICAL_TIME_ERR_OVERFLOW;
    }
    return musical_time_create((long)sum.value.num, (unsigned long)sum.value.den, out);
}
