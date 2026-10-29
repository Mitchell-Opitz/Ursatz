#ifndef MUSIC_TIME_DURATION_H
#define MUSIC_TIME_DURATION_H

#include <stdbool.h>

/*
 * Duration: an exact rational magnitude, expressed as numerator/denominator
 * in lowest terms. Immutable, never represented as float/double (Law 19).
 *
 * A Duration is a magnitude, not a position — it is structurally similar
 * to MusicalTime but not interchangeable/convertible with it.
 */

typedef struct Duration Duration;

typedef enum {
    DURATION_OK = 0,
    DURATION_ERR_NON_POSITIVE_DENOMINATOR,
    DURATION_ERR_NEGATIVE,
    DURATION_ERR_ALLOC,
    DURATION_ERR_OVERFLOW /* an arithmetic result does not fit the numerator or denominator */
} DurationResult;

/* Reduces numerator/denominator to lowest terms at construction. */
DurationResult duration_create(long numerator, unsigned long denominator, Duration **out);
void duration_destroy(Duration *duration);

/* Value equality on reduced value: Duration(3,4) == Duration(6,8). */
bool duration_equals(const Duration *a, const Duration *b);

long duration_numerator(const Duration *duration);
unsigned long duration_denominator(const Duration *duration);

DurationResult duration_add(const Duration *a, const Duration *b, Duration **out);
/* Fails with DURATION_ERR_NEGATIVE if a - b would be negative. */
DurationResult duration_subtract(const Duration *a, const Duration *b, Duration **out);

#endif