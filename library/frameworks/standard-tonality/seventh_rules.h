#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_RULES_H

#include <stdbool.h>

#include "model/pitch/pitch_spelling.h"
#include "triad_rules.h"

/*
 * Seventh chord structure rules for standard-tonality-v1. A seventh
 * chord is a four-note chord whose notes can be arranged in thirds:
 *
 *   STACKED_IN_THIRDS      stacked in its most compact form (an extra-long
 *                          snowperson), a seventh chord has a lowest
 *                          note, a note a generic third above it, one a
 *                          generic fifth above, and one a generic
 *                          seventh above
 *   LINES_OR_SPACES        a stacked seventh chord's notes are always all
 *                          on lines or all on spaces
 *   ROOT_THIRD_FIFTH_SEVENTH
 *                          in that form the lowest note is the root, the
 *                          next the third, then the fifth, and the
 *                          highest the seventh (the "chordal" seventh,
 *                          as distinct from the seventh scale degree)
 *   QUALITIES              five common qualities, set by two things: the
 *                          quality of the triad of root, third, and
 *                          fifth, and the quality of the seventh from
 *                          the root: major-major (major triad, major
 *                          seventh), major-minor (major triad, minor
 *                          seventh), minor-minor (minor triad, minor
 *                          seventh), half-diminished (diminished triad,
 *                          minor seventh), fully diminished (diminished
 *                          triad, diminished seventh)
 *   NAMING                 for the first three the first word names the
 *                          triad and the second the seventh; the two
 *                          diminished-triad qualities differ only in
 *                          the seventh
 *   ALTERNATE_NAMES        the major-major seventh chord is also called
 *                          the major seventh chord, the major-minor the
 *                          dominant seventh chord, the minor-minor the
 *                          minor seventh chord, and the fully diminished
 *                          the diminished seventh chord; the half-
 *                          diminished has no alternate name
 *
 * Measured in semitones from the root, with the triad rules (triad_rules.h).
 *
 * Chromatically altered seventh chords (an augmented triad with a
 * seventh, a minor triad with a major seventh) are outside the five and
 * are a later chapter's: the quality functions report no answer for
 * them.
 */

typedef enum {
    STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR = 0,
    STANDARD_TONALITY_SEVENTH_MAJOR_MINOR,
    STANDARD_TONALITY_SEVENTH_MINOR_MINOR,
    STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED,
    STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED,
    STANDARD_TONALITY_SEVENTH_QUALITY_COUNT
} StandardTonalitySeventhQuality;

/* QUALITIES: the root, third, fifth and seventh of a seventh chord of the quality as semitones above the
   root, in StandardTonalitySeventhQuality order: 0 4 7 11, 0 4 7 10, 0 3 7 10, 0 3 6 10, 0 3 6 9. False (out
   untouched) for an invalid quality or NULL out. */
bool standard_tonality_seventh_quality_semitones(StandardTonalitySeventhQuality quality, int out[4]);

#endif
