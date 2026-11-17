#include "model/time/musical_time.h"

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
