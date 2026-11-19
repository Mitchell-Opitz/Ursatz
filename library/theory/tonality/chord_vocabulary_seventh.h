#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_SEVENTH_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_SEVENTH_H

#include <stdbool.h>

#include "theory/tonality/chord_vocabulary.h"

/*
 * Seventh-chord vocabulary for common-practice-minimal-v1: the four
 * seventh-chord qualities that occur diatonically within a major or
 * natural-minor key -- dominant7, major7, minor7, half-diminished7 --
 * and the fully-diminished7 of harmonic minor (the raised leading tone's
 * seventh). Deliberately excludes chromatic/secondary-dominant
 * sevenths and any quality resolution that would need a scale degree:
 * matching is by root-relative interval content, not by scale degree.
 */

typedef enum {
    COMMON_PRACTICE_SEVENTH_DOMINANT7 = 0,
    COMMON_PRACTICE_SEVENTH_MAJOR7,
    COMMON_PRACTICE_SEVENTH_MINOR7,
    COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7,
    COMMON_PRACTICE_SEVENTH_DIMINISHED7,
    COMMON_PRACTICE_SEVENTH_COUNT
} CommonPracticeSeventh;

/* The first four sevenths are the diatonic set of a major key. The fully-diminished seventh is the harmonic-minor
   vii-dim-7. */
#define COMMON_PRACTICE_SEVENTH_MAJOR_KEY_COUNT 4

/*
 * The quality as root-relative semitones, ascending, written to out[4]: dominant7 {0,4,7,10}, major7
 * {0,4,7,11}, minor7 {0,3,7,10}, half-diminished7 {0,3,6,10}, diminished7 {0,3,6,9}. False (out untouched)
 * for an invalid seventh.
 */
bool common_practice_seventh_intervals(CommonPracticeSeventh seventh, int out[4]);

/* Quality label this framework assigns, e.g. "dominant7", "half-diminished7". */
const char *common_practice_seventh_label(CommonPracticeSeventh seventh);

/*
 * The candidate that names a sonority this seventh chord: its label and the confidence confidence_numerator / confidence_denominator (a
 * rational in [0, 1], reduced), with no structure. False (out untouched) for an invalid seventh or a zero denominator.
 */
bool common_practice_seventh_candidate(CommonPracticeSeventh seventh, unsigned long confidence_numerator,
                                       unsigned long confidence_denominator, ChordCandidate *out);

#endif
