#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"

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
 * As common_practice_build_triad_chord, but the Interpretation's scalar confidence is
 * confidence_numerator / confidence_denominator (a rational in [0, 1]) instead of 1, and the
 * Interpretation's id has interpretation_id_suffix (NULL for none) appended, so two Chords over one
 * Sonority (a best and a runner-up) get distinct ids. The existing builder is this one with confidence
 * 1/1 and no suffix. Takes ownership of sonority_target_id on success only.
 */
ChordResult common_practice_build_triad_chord_with_confidence(CommonPracticeTriad triad, EntityID *sonority_target_id,
                                                          unsigned long confidence_numerator,
                                                          unsigned long confidence_denominator,
                                                          const char *interpretation_id_suffix, Chord **out);

#endif
