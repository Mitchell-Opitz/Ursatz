#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_INTERVAL_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_INTERVAL_H

#include <stdbool.h>

#include "model/pitch/pitch_spelling.h"

/*
 * The interval between two pitches, for standard-tonality-v1: its
 * generic size and its quality. Direction is ignored, so the interval
 * from C4 up to G4 and the one from G4 down to C4 are the same perfect
 * fifth.
 *
 *   size          the generic size counted in letter names: unison 1,
 *                 second 2, ... octave 8, twelfth 12
 *   simple_size   the same interval with whole octaves removed, 1 to 8
 *                 (a twelfth is a simple fifth; an octave stays 8, a
 *                 unison stays 1)
 *   quality       the quality of the simple interval. Unisons, fourths,
 *                 fifths, and octaves are perfect, augmented, or
 *                 diminished; seconds, thirds, sixths, and sevenths are
 *                 major, minor, augmented, or diminished. Doubly
 *                 augmented and diminished intervals are read as
 *                 augmented and diminished.
 */

typedef enum {
    STANDARD_TONALITY_INTERVAL_QUALITY_PERFECT = 0,
    STANDARD_TONALITY_INTERVAL_QUALITY_MAJOR,
    STANDARD_TONALITY_INTERVAL_QUALITY_MINOR,
    STANDARD_TONALITY_INTERVAL_QUALITY_AUGMENTED,
    STANDARD_TONALITY_INTERVAL_QUALITY_DIMINISHED
} StandardTonalityIntervalQuality;

typedef struct {
    int size;
    int simple_size;
    StandardTonalityIntervalQuality quality;
} StandardTonalityInterval;

/* The interval between the two pitches. False (*out untouched) for NULL,
   or for spellings so unusual that the higher-sounding one has fewer
   letter steps than the lower one. */
bool standard_tonality_interval_between(const PitchSpelling *a, const PitchSpelling *b, StandardTonalityInterval *out);

/* The interval of a generic size 1 or more (simple_size by whole octaves removed, an octave stays 8). Its quality is read from no
   pitches, so it is PERFECT: a caller that needs the quality uses standard_tonality_interval_between. A size under 1 gives a
   unison. */
StandardTonalityInterval standard_tonality_interval_of_size(int size);

/* The generic size a number of semitones (either direction) implies when no letter names are known: 0 is a unison (1), 1 and 2 a
   second (2), 3 and 4 a third, 5 and 6 a fourth, 7 a fifth, 8 and 9 a sixth, 10 and 11 a seventh, then the same again an octave
   up (12 is an octave, 8). */
int standard_tonality_generic_size_of_semitones(long semitones);

/* True for a second (generic size 2), false otherwise. A step is a minor
   or major second (or an augmented or diminished one), in either direction. */
bool standard_tonality_interval_is_step(const StandardTonalityInterval *interval);

/* True for any interval of generic size 3 or more, compound included. A
   unison and a second are not leaps. False for NULL. */
bool standard_tonality_interval_is_leap(const StandardTonalityInterval *interval);

#endif
