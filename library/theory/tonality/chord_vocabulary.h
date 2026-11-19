#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_H

#include <stdbool.h>

#include <stddef.h>

#include "model/core/rational.h"

#define CHORD_STRUCTURE_MAX_INTERVALS 8

/*
 * The shape of a chord candidate in numbers: the root's pitch class (0 = C ... 11 = B), the quality as the root-relative interval set in
 * semitones, ascending, starting at 0 (a major triad {0,4,7}, a dominant seventh {0,4,7,10}), and the bass's pitch class. Any of the
 * three may be unknown: a pitch class of -1, or an interval_count of 0.
 */
typedef struct {
    int root_pitch_class;
    int bass_pitch_class;
    size_t interval_count;
    int root_relative_intervals[CHORD_STRUCTURE_MAX_INTERVALS];
} ChordStructure;

/*
 * A chord candidate: the label the vocabulary gives (e.g. "V", "dominant7"), the confidence in it (from 0 to 1) and, once the identification
 * has read it, its structure (unknown, root and bass -1 and no intervals, as the vocabulary gives it).
 */
typedef struct {
    char label[32];
    Rational confidence;
    ChordStructure structure;
} ChordCandidate;

/*
 * Chord vocabulary for common-practice-minimal-v1: exactly the seven
 * diatonic triads of Western common-practice tonal harmony, relative to
 * a declared key. Deliberately excludes jazz extensions, modal
 * interchange, secondary dominants, and borrowed chords -- see
 * README.md.
 *
 * Matching an actual Sonority against this vocabulary is
 * weighted_chord_identification.h's job.
 */

typedef enum {
    COMMON_PRACTICE_TRIAD_I = 0,
    COMMON_PRACTICE_TRIAD_II,
    COMMON_PRACTICE_TRIAD_III,
    COMMON_PRACTICE_TRIAD_IV,
    COMMON_PRACTICE_TRIAD_V,
    COMMON_PRACTICE_TRIAD_VI,
    COMMON_PRACTICE_TRIAD_VII_DIM,
    COMMON_PRACTICE_TRIAD_AUGMENTED,
    COMMON_PRACTICE_TRIAD_MAJOR,
    COMMON_PRACTICE_TRIAD_MINOR,
    COMMON_PRACTICE_TRIAD_DIMINISHED,
    COMMON_PRACTICE_TRIAD_COUNT
} CommonPracticeTriad;

/* The first seven triads are the degree-indexed diatonic set of a major key. Later entries are qualities that no major key
   gives. */
#define COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT 7

/*
 * The triad's quality as root-relative semitones, ascending, written to out[3]: I IV V major {0,4,7},
 * ii iii vi minor {0,3,7}, vii-dim diminished {0,3,6}, augmented {0,4,8}. False (out untouched) for an
 * invalid triad.
 */
bool common_practice_triad_intervals(CommonPracticeTriad triad, int out[3]);

/*
 * The candidate that names a sonority this triad: its label and the confidence confidence_numerator / confidence_denominator (a rational in
 * [0, 1], reduced), with no structure. False (out untouched) for an invalid triad or a zero denominator.
 */
bool common_practice_triad_candidate(CommonPracticeTriad triad, unsigned long confidence_numerator,
                                     unsigned long confidence_denominator, ChordCandidate *out);

#endif
