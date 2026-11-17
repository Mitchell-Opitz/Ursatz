#ifndef MUSIC_TIME_DURATION_H
#define MUSIC_TIME_DURATION_H

#include <stdbool.h>
#include "model/core/rational.h"

/*
 * Duration: an exact rational magnitude over a checked Rational (lowest
 * terms, den > 0, value >= 0). A plain value: copy it, compare it with
 * duration_cmp or duration_same, never allocate it. Never represented as
 * float/double (Law 19).
 *
 * A Duration is a magnitude, not a position — it is structurally similar
 * to MusicalTime but not interchangeable/convertible with it.
 */

typedef struct {
    Rational value;
} Duration;

typedef enum {
    DURATION_OK = 0,
    DURATION_ERR_NON_POSITIVE_DENOMINATOR,
    DURATION_ERR_NEGATIVE,
    DURATION_ERR_ALLOC,
    DURATION_ERR_OVERFLOW /* an arithmetic result does not fit the numerator or denominator */
} DurationResult;

/* Value API. DURATION_ERR_NEGATIVE for a negative value. */
DurationResult duration_make(long long num, long long den, Duration *out);
DurationResult duration_of(Rational value, Duration *out);
/* -1, 0 or 1; overflow-free. */
int duration_cmp(Duration a, Duration b);
bool duration_same(Duration a, Duration b);
DurationResult duration_plus(Duration a, Duration b, Duration *out);

/* Heap API, kept until the callers are converted (one allocation per object). A denominator above
   LLONG_MAX is DURATION_ERR_OVERFLOW. Reduces numerator/denominator to lowest terms at construction. */
DurationResult duration_create(long numerator, unsigned long denominator, Duration **out);
void duration_destroy(Duration *duration);

/* Value equality on reduced value: Duration(3,4) == Duration(6,8). */
bool duration_equals(const Duration *a, const Duration *b);

long duration_numerator(const Duration *duration);
unsigned long duration_denominator(const Duration *duration);

DurationResult duration_add(const Duration *a, const Duration *b, Duration **out);

#endif