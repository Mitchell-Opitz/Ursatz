#ifndef MUSIC_TIME_MUSICAL_TIME_H
#define MUSIC_TIME_MUSICAL_TIME_H

#include <stdbool.h>
#include "time/duration.h"

/*
 * MusicalTime: an exact rational position, expressed as numerator/
 * denominator in lowest terms. Immutable, never represented as float/
 * double (Law 19).
 *
 * Structurally identical to Duration but semantically a POSITION, not a
 * magnitude. MusicalTime and Duration are NOT interchangeable/convertible
 * even though the underlying rational representation coincides.
 *
 * Unlike Duration, no lower bound is enforced here: negative values (e.g.
 * pickup measures) are legal at this layer. Semantics are deferred to a
 * later spec.
 */

typedef struct MusicalTime MusicalTime;

typedef enum {
    MUSICAL_TIME_OK = 0,
    MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR,
    MUSICAL_TIME_ERR_ALLOC,
    MUSICAL_TIME_ERR_OVERFLOW /* an arithmetic result does not fit the numerator or denominator */
} MusicalTimeResult;

MusicalTimeResult musical_time_create(long numerator, unsigned long denominator, MusicalTime **out);
void musical_time_destroy(MusicalTime *time);

bool musical_time_equals(const MusicalTime *a, const MusicalTime *b);

long musical_time_numerator(const MusicalTime *time);
unsigned long musical_time_denominator(const MusicalTime *time);

MusicalTimeResult musical_time_add(const MusicalTime *time, const Duration *duration, MusicalTime **out);
DurationResult musical_time_subtract(const MusicalTime *a, const MusicalTime *b, Duration **out);

#endif