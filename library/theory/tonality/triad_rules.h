#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TRIAD_RULES_H

#include <stdbool.h>

#include "model/pitch/pitch_spelling.h"

/*
 * Triad structure rules for standard-tonality-v1. A chord is any
 * combination of three or more pitch classes sounding together; a triad
 * is a three-note chord whose notes can be arranged in thirds:
 *
 *   STACKED_IN_THIRDS   stacked in its most compact form (a snowperson),
 *                       a triad has a lowest note, a middle note a
 *                       generic third above it, and a highest note a
 *                       generic fifth above it
 *   LINES_OR_SPACES     a stacked triad's notes are always all on lines
 *                       or all on spaces
 *   ROOT_THIRD_FIFTH    in that form the lowest note is the root, the
 *                       middle the third, the highest the fifth (the
 *                       form is root position, taken up in Inversion)
 *   QUALITIES           four qualities, set by the intervals from the root
 *                       to the third and to the fifth: major (major
 *                       third, perfect fifth), minor (minor third,
 *                       perfect fifth), diminished (minor third,
 *                       diminished fifth), augmented (major third,
 *                       augmented fifth)
 *   NAMING              major and minor triads are named for the quality
 *                       of their third (both fifths are perfect);
 *                       diminished and augmented for the quality of
 *                       their fifth (the diminished third is minor, the
 *                       augmented third major)
 *
 * Measured in semitones from the root.
 */

typedef enum {
    STANDARD_TONALITY_TRIAD_MAJOR = 0,
    STANDARD_TONALITY_TRIAD_MINOR,
    STANDARD_TONALITY_TRIAD_DIMINISHED,
    STANDARD_TONALITY_TRIAD_AUGMENTED,
    STANDARD_TONALITY_TRIAD_QUALITY_COUNT
} StandardTonalityTriadQuality;

/* QUALITIES: the root, third and fifth of a triad of the quality as semitones above the root: 0 4 7, 0 3 7,
   0 3 6, 0 4 8. False (out untouched) for an invalid quality or NULL out. */
bool standard_tonality_triad_quality_semitones(StandardTonalityTriadQuality quality, int out[3]);

#endif
