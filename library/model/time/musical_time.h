#ifndef MUSIC_TIME_MUSICAL_TIME_H
#define MUSIC_TIME_MUSICAL_TIME_H

#include <stdbool.h>
#include "model/core/rational.h"
#include "model/time/duration.h"

/*
 * MusicalTime: an exact rational position over a checked Rational (lowest
 * terms, den > 0, any sign). A plain value: copy it, compare it with
 * musical_time_cmp or musical_time_same, never allocate it. Never
 * represented as float/double (Law 19). A struct literal with the same
 * Rational equals the made value.
 *
 * Structurally identical to Duration but semantically a POSITION, not a
 * magnitude. MusicalTime and Duration are NOT interchangeable/convertible
 * even though the underlying rational representation coincides.
 *
 * Unlike Duration, no lower bound is enforced here: negative values (e.g.
 * pickup measures) are legal at this layer. Semantics are deferred to a
 * later spec.
 */

typedef struct {
    Rational value;
} MusicalTime;

typedef enum {
    MUSICAL_TIME_OK = 0,
    MUSICAL_TIME_ERR_NON_POSITIVE_DENOMINATOR,
    MUSICAL_TIME_ERR_ALLOC,
    MUSICAL_TIME_ERR_OVERFLOW /* an arithmetic result does not fit the numerator or denominator */
} MusicalTimeResult;

/* Value API. */
MusicalTimeResult musical_time_make(long long num, long long den, MusicalTime *out);
static inline MusicalTime musical_time_of(Rational value) {
    MusicalTime t = {value};
    return t;
}
/* -1, 0 or 1; overflow-free. */
int musical_time_cmp(MusicalTime a, MusicalTime b);
bool musical_time_same(MusicalTime a, MusicalTime b);
/* t + d; MUSICAL_TIME_ERR_OVERFLOW when the sum does not fit. Nothing is written on failure. */
MusicalTimeResult musical_time_plus(MusicalTime t, Duration d, MusicalTime *out);

/* Heap API, kept until the callers are converted (one allocation per object). A denominator above
   LLONG_MAX is MUSICAL_TIME_ERR_OVERFLOW. */
MusicalTimeResult musical_time_create(long numerator, unsigned long denominator, MusicalTime **out);
void musical_time_destroy(MusicalTime *time);

bool musical_time_equals(const MusicalTime *a, const MusicalTime *b);

long musical_time_numerator(const MusicalTime *time);
unsigned long musical_time_denominator(const MusicalTime *time);

MusicalTimeResult musical_time_add(const MusicalTime *time, const Duration *duration, MusicalTime **out);

#endif