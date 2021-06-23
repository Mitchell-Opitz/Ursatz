#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_INTERVAL_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_INTERVAL_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * The interval between two pitches, for standard-counterpoint-v1: its
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
    STANDARD_COUNTERPOINT_QUALITY_PERFECT = 0,
    STANDARD_COUNTERPOINT_QUALITY_MAJOR,
    STANDARD_COUNTERPOINT_QUALITY_MINOR,
    STANDARD_COUNTERPOINT_QUALITY_AUGMENTED,
    STANDARD_COUNTERPOINT_QUALITY_DIMINISHED
} StandardCounterpointQuality;

typedef struct {
    int size;
    int simple_size;
    StandardCounterpointQuality quality;
} StandardCounterpointInterval;

/* The interval between the two pitches. False (*out untouched) for NULL,
   or for spellings so unusual that the higher-sounding one has fewer
   letter steps than the lower one. */
bool standard_counterpoint_interval_between(const PitchSpelling *a, const PitchSpelling *b,
                                            StandardCounterpointInterval *out);

/* True for a second (generic size 2), false otherwise. A step is a minor
   or major second (or an augmented or diminished one), in either direction. */
bool standard_counterpoint_interval_is_step(const StandardCounterpointInterval *interval);

/* True for any interval of generic size 3 or more, compound included. A
   unison and a second are not leaps. False for NULL. */
bool standard_counterpoint_interval_is_leap(const StandardCounterpointInterval *interval);

#endif
