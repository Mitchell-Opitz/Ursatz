#ifndef MUSIC_TIME_PERFORMANCE_TIME_H
#define MUSIC_TIME_PERFORMANCE_TIME_H

#include <stdbool.h>

/*
 * PerformanceTime: a count of seconds of performance time, as an exact rational (numerator over
 * denominator, in lowest terms). It is a pure value with no theory: it does not know about notes,
 * tempo, or MusicalTime. TempoMap (time/tempo_map.h) is what turns a MusicalTime into one.
 *
 * It is never a floating-point number, and it is never negative: it is a count of seconds from
 * the start (or a length of seconds), as Duration is a length of whole notes. The two are kept
 * apart on purpose. A Duration is symbolic time, in whole notes, and does not depend on tempo; a
 * PerformanceTime is what a tempo makes of it.
 *
 * Value type: immutable, value equality on the lowest-terms pair. Arithmetic is exact and checked:
 * a result whose numerator or denominator does not fit is PERFORMANCE_TIME_ERR_OVERFLOW, never a
 * wrapped value.
 */

typedef struct PerformanceTime PerformanceTime;

typedef enum {
    PERFORMANCE_TIME_OK = 0,
    PERFORMANCE_TIME_ERR_NON_POSITIVE_DENOMINATOR,
    PERFORMANCE_TIME_ERR_NEGATIVE,
    PERFORMANCE_TIME_ERR_OVERFLOW,
    PERFORMANCE_TIME_ERR_NULL_ARGUMENT,
    PERFORMANCE_TIME_ERR_ALLOC
} PerformanceTimeResult;

/* numerator >= 0, denominator >= 1. The pair is reduced to lowest terms. */
PerformanceTimeResult performance_time_create(long numerator, unsigned long denominator, PerformanceTime **out);
void performance_time_destroy(PerformanceTime *time);

long performance_time_numerator(const PerformanceTime *time);
unsigned long performance_time_denominator(const PerformanceTime *time);

bool performance_time_equals(const PerformanceTime *a, const PerformanceTime *b);

/* -1, 0 or 1 as a is less than, equal to or greater than b. Exact, and cannot overflow. */
int performance_time_compare(const PerformanceTime *a, const PerformanceTime *b);

/* a + b, exact. PERFORMANCE_TIME_ERR_OVERFLOW if the sum does not fit. */
PerformanceTimeResult performance_time_add(const PerformanceTime *a, const PerformanceTime *b,
                                            PerformanceTime **out);

#endif
