#include "model/time/duration.h"

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
