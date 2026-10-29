#ifndef MUSIC_TIME_TIME_RATIONAL_H
#define MUSIC_TIME_TIME_RATIONAL_H

#include "core/rational.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/performance_time.h"

/*
 * Bridges between the signed Rational (core/rational.h) and the time value types. Reading a value is
 * exact; it fails with RATIONAL_ERR_OVERFLOW only when a field does not fit a long long. Building a
 * value is a new owned object, released with its own destroy; it fails when the rational does not fit
 * the target's field widths, or is negative for a Duration. Nothing is written to *out on failure.
 */

typedef enum {
    TIME_RATIONAL_OK = 0,
    TIME_RATIONAL_ERR_NULL,
    TIME_RATIONAL_ERR_OVERFLOW,
    TIME_RATIONAL_ERR_NEGATIVE,
    TIME_RATIONAL_ERR_ALLOC
} TimeRationalResult;

RationalResult rational_of_musical_time(const MusicalTime *time, Rational *out);
RationalResult rational_of_duration(const Duration *duration, Rational *out);
RationalResult rational_of_performance_time(const PerformanceTime *time, Rational *out);

TimeRationalResult musical_time_from_rational(Rational value, MusicalTime **out);
/* A negative value is TIME_RATIONAL_ERR_NEGATIVE. */
TimeRationalResult duration_from_rational(Rational value, Duration **out);

/* A new MusicalTime with this value, or NULL when it does not fit or on allocation failure. */
MusicalTime *musical_time_from_rational_or_null(Rational value);

#endif
