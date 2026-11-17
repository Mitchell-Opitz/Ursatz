#include "model/time/time_rational.h"

#include <stddef.h>

RationalResult rational_of_musical_time(const MusicalTime *time, Rational *out) {
    if (time == NULL || out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    *out = time->value;
    return RATIONAL_OK;
}

RationalResult rational_of_duration(const Duration *duration, Rational *out) {
    if (duration == NULL || out == NULL) {
        return RATIONAL_ERR_NULL;
    }
    *out = duration->value;
    return RATIONAL_OK;
}

TimeRationalResult musical_time_from_rational(Rational value, MusicalTime **out) {
    if (out == NULL) {
        return TIME_RATIONAL_ERR_NULL;
    }
    MusicalTime *time = NULL;
    switch (musical_time_create((long)value.num, (unsigned long)value.den, &time)) {
    case MUSICAL_TIME_OK:
        break;
    case MUSICAL_TIME_ERR_ALLOC:
        return TIME_RATIONAL_ERR_ALLOC;
    default:
        return TIME_RATIONAL_ERR_OVERFLOW;
    }
    *out = time;
    return TIME_RATIONAL_OK;
}

MusicalTime *musical_time_from_rational_or_null(Rational value) {
    MusicalTime *time = NULL;
    return musical_time_from_rational(value, &time) == TIME_RATIONAL_OK ? time : NULL;
}
