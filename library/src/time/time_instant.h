#ifndef MUSIC_TIME_TIME_INSTANT_H
#define MUSIC_TIME_TIME_INSTANT_H

#include <stdbool.h>
#include "time/musical_time.h"

/*
 * TimeInstant: a zero-duration temporal point, wrapping MusicalTime. Used
 * for instantaneous events (tempo change, meter change, key-signature
 * declaration) as distinct from TimeSpan.
 *
 * Exact rational, immutable, never float/double (Law 19). No lower bound
 * enforced at kernel level, matching MusicalTime.
 */

typedef struct TimeInstant TimeInstant;

typedef enum {
    TIME_INSTANT_OK = 0,
    TIME_INSTANT_ERR_NON_POSITIVE_DENOMINATOR,
    TIME_INSTANT_ERR_ALLOC
} TimeInstantResult;

/* Reduces numerator/denominator to lowest terms at construction. */
TimeInstantResult time_instant_create(long numerator, unsigned long denominator, TimeInstant **out);
void time_instant_destroy(TimeInstant *instant);

/* Value equality on reduced value. */
bool time_instant_equals(const TimeInstant *a, const TimeInstant *b);

long time_instant_numerator(const TimeInstant *instant);
unsigned long time_instant_denominator(const TimeInstant *instant);
const MusicalTime *time_instant_time(const TimeInstant *instant);

#endif