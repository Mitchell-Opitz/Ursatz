#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_SIZE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_INTERVAL_SIZE_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Interval size rules for standard-tonality-v1. Two pitches form an
 * interval: melodic when played or sung separately, harmonic when
 * together. Every interval has a size and a quality; this file is size
 * (quality is interval_quality_rules.h):
 *
 *   COUNTING   size counts the lines and spaces from one note to the
 *              other, beginning with one: a note to itself is 1, to the
 *              next letter 2, and so on
 *   NAMES      written with numbers, spoken with ordinals: second,
 *              third, fourth, ... -- except that size 1 is a "unison",
 *              not a "first", and size 8 an "octave", not an "eighth"
 *   GENERIC    size is generic: accidentals never change it (C-sharp
 *              to E-flat is a third, like C to E)
 *   COMPOUND   an interval of an octave or smaller is simple; a larger
 *              one is compound. Add 7 to a simple size to make it
 *              compound: unison 1 becomes octave 8, 2 becomes 9, 3
 *              becomes 10, 4 becomes 11, 5 becomes 12, 6 becomes 13
 *
 * Sizes are measured from the lower note up (GenericInterval,
 * pitch/generic_interval.h).
 */

#define STANDARD_TONALITY_INTERVAL_OCTAVE 8

typedef enum {
    STANDARD_TONALITY_INTERVAL_MELODIC = 0,
    STANDARD_TONALITY_INTERVAL_HARMONIC
} StandardTonalityIntervalType;

typedef enum {
    STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNTING = 0,
    STANDARD_TONALITY_INTERVAL_SIZE_RULE_NAMES,
    STANDARD_TONALITY_INTERVAL_SIZE_RULE_GENERIC,
    STANDARD_TONALITY_INTERVAL_SIZE_RULE_COMPOUND,
    STANDARD_TONALITY_INTERVAL_SIZE_RULE_COUNT
} StandardTonalityIntervalSizeRule;

const Rule *standard_tonality_interval_size_rule(StandardTonalityIntervalSizeRule which);
const Constraint *standard_tonality_interval_size_constraint(StandardTonalityIntervalSizeRule which);

/* NAMES: "unison" "second" "third" "fourth" "fifth" "sixth" "seventh"
   "octave" "ninth" "tenth" "eleventh" "twelfth" "thirteenth" for sizes
   1..13. NULL for any other size. */
const char *standard_tonality_interval_size_name(int size);

/* COUNTING / GENERIC: the size from `lower` up to `upper`, by letter and
   octave only (accidentals ignored). On true *size is >= 1. False
   (output untouched) for NULL, a NULL output, or an upper note below the
   lower on the staff. */
bool standard_tonality_interval_generic_size(const PitchSpelling *lower, const PitchSpelling *upper, int *size);

/* COMPOUND: true for size 1..8. False for any other size. */
bool standard_tonality_interval_is_simple(int size);

/* COMPOUND: true for size > 8. */
bool standard_tonality_interval_is_compound(int size);

/* COMPOUND: the simple size of a size >= 1: 1..8, octaves removed (10 is
   3, 15 is 8). 0 for a size below 1. */
int standard_tonality_interval_simple_size(int size);

/* COMPOUND: a simple size made compound by adding 7 (1 to 8, 2 to 9, 3
   to 10 ...). 0 for a size outside 1..8. */
int standard_tonality_interval_compound_size(int size);

#endif
