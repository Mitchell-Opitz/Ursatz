#include "time/time_rational.h"

#include <limits.h>
#include <stddef.h>

static RationalResult make_from_fields(long numerator, unsigned long denominator, Rational *out) {
    if (out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    if (denominator > (unsigned long)LLONG_MAX) {
        return RATIONAL_ERR_OVERFLOW;
    }
    return rational_make((long long)numerator, (long long)denominator, out);
}

RationalResult rational_of_musical_time(const MusicalTime *time, Rational *out) {
    if (time == NULL) {
        return RATIONAL_ERR_NULL;
    }
    return make_from_fields(musical_time_numerator(time), musical_time_denominator(time), out);
}

RationalResult rational_of_duration(const Duration *duration, Rational *out) {
    if (duration == NULL) {
        return RATIONAL_ERR_NULL;
    }
    return make_from_fields(duration_numerator(duration), duration_denominator(duration), out);
}

RationalResult rational_of_performance_time(const PerformanceTime *time, Rational *out) {
    if (time == NULL) {
        return RATIONAL_ERR_NULL;
    }
    return make_from_fields(performance_time_numerator(time), performance_time_denominator(time), out);
}

static bool fits_fields(Rational value) {
    return value.num >= LONG_MIN && value.num <= LONG_MAX && (unsigned long long)value.den <= ULONG_MAX;
}

TimeRationalResult musical_time_from_rational(Rational value, MusicalTime **out) {
    if (out == NULL) {
        return TIME_RATIONAL_ERR_NULL;
    }
    if (!fits_fields(value)) {
        return TIME_RATIONAL_ERR_OVERFLOW;
    }
    MusicalTime *time = NULL;
    if (musical_time_create((long)value.num, (unsigned long)value.den, &time) != MUSICAL_TIME_OK) {
        return TIME_RATIONAL_ERR_ALLOC;
    }
    *out = time;
    return TIME_RATIONAL_OK;
}

MusicalTime *musical_time_from_rational_or_null(Rational value) {
    MusicalTime *time = NULL;
    return musical_time_from_rational(value, &time) == TIME_RATIONAL_OK ? time : NULL;
}

TimeRationalResult duration_from_rational(Rational value, Duration **out) {
    if (out == NULL) {
        return TIME_RATIONAL_ERR_NULL;
    }
    if (rational_is_negative(value)) {
        return TIME_RATIONAL_ERR_NEGATIVE;
    }
    if (!fits_fields(value)) {
        return TIME_RATIONAL_ERR_OVERFLOW;
    }
    Duration *duration = NULL;
    if (duration_create((long)value.num, (unsigned long)value.den, &duration) != DURATION_OK) {
        return TIME_RATIONAL_ERR_ALLOC;
    }
    *out = duration;
    return TIME_RATIONAL_OK;
}
