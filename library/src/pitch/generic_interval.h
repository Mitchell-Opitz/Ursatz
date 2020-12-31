#ifndef MUSIC_PITCH_GENERIC_INTERVAL_H
#define MUSIC_PITCH_GENERIC_INTERVAL_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * GenericInterval (Value): the size of an interval counted in letter
 * names -- the number of lines and spaces from one written note to
 * another, counting both ends: a note and itself is 1, C to G is 5.
 * "Generic" because accidentals play no part: C-sharp to G-flat and C to
 * G are both fifths. Quality (perfect, major, minor, augmented,
 * diminished) is the specific counterpart and is a rule of a Framework
 * (frameworks/standard-tonality), not part of this type.
 *
 * size is at least 1. It is a letter count, so it is also theory-
 * neutral: the same for any tuning or key. An interval no larger than
 * an octave (size <= 8) is simple; a larger one is compound, and
 * adding an octave adds 7 to its size (a third, 3, becomes a tenth,
 * 10).
 *
 * This is distinct from Interval (semantics/interval.h), which carries
 * semitone distances between opaque PitchIdentity tokens and does not
 * represent diatonic distance.
 *
 * Value type: immutable, value equality on size.
 */

typedef struct GenericInterval GenericInterval;

typedef enum {
    GENERIC_INTERVAL_OK = 0,
    GENERIC_INTERVAL_ERR_INVALID_SIZE,
    GENERIC_INTERVAL_ERR_NULL_SPELLING,
    GENERIC_INTERVAL_ERR_UPPER_BELOW_LOWER,
    GENERIC_INTERVAL_ERR_ALLOC
} GenericIntervalResult;

/* The number of letters in an octave: simple intervals run 1..8 and
   adding an octave adds this many less one. */
#define GENERIC_INTERVAL_OCTAVE_SIZE 8
#define GENERIC_INTERVAL_LETTERS_PER_OCTAVE 7

/* size must be >= 1. */
GenericIntervalResult generic_interval_create(int size, GenericInterval **out);

/*
 * The generic interval from `lower` up to `upper`: the count of lines
 * and spaces, ends included, by letter and octave only. upper must not
 * be below lower on the staff (a note on the same line or space is a
 * unison, size 1); otherwise GENERIC_INTERVAL_ERR_UPPER_BELOW_LOWER.
 */
GenericIntervalResult generic_interval_between(const PitchSpelling *lower, const PitchSpelling *upper,
                                               GenericInterval **out);

void generic_interval_destroy(GenericInterval *interval);

int generic_interval_size(const GenericInterval *interval);

/* Size 8 or smaller. */
bool generic_interval_is_simple(const GenericInterval *interval);
bool generic_interval_is_compound(const GenericInterval *interval);

/* The size with octaves removed, 1..8: a tenth (10) is a third (3); an
   octave (8) stays an octave. Simple intervals are returned unchanged. */
int generic_interval_simple_size(const GenericInterval *interval);

/* A new GenericInterval an octave larger (size + 7). */
GenericIntervalResult generic_interval_add_octave(const GenericInterval *interval, GenericInterval **out);

/* Value equality on size. */
bool generic_interval_equals(const GenericInterval *a, const GenericInterval *b);

#endif
