#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Chord vocabulary for common-practice-minimal-v1: exactly the seven
 * diatonic triads of Western common-practice tonal harmony, relative to
 * a declared key. Deliberately excludes jazz extensions, modal
 * interchange, secondary dominants, and borrowed chords -- see
 * README.md.
 *
 * Each triad has one Rule and one Constraint (CONSTRAINT_TYPE_HARD --
 * a Sonority either is or is not this triad, no weighting) referencing
 * the shared common_practice_minimal_framework(). This module never
 * calls constraint_evaluate: that stub stays untouched, reserved for
 * real L8 generation-pipeline work (docs/modules/rules.md). Matching an
 * actual Sonority against this vocabulary is triad_identification.h's
 * job, built on top of this file's label/Rule/Constraint accessors.
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

/* The first seven triads are the degree-indexed diatonic set of a major key: the subset matcher
   (triad_identification.h) scans only these. Later entries are qualities that no major key gives. */
#define COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT 7

/*
 * The triad's quality as root-relative semitones, ascending, written to out[3]: I IV V major {0,4,7},
 * ii iii vi minor {0,3,7}, vii-dim diminished {0,3,6}, augmented {0,4,8}. False (out untouched) for an
 * invalid triad.
 */
bool common_practice_triad_intervals(CommonPracticeTriad triad, int out[3]);

/* Roman-numeral label this framework assigns, e.g. "I", "ii", "vii\xC2\xB0"; the augmented triad, which no
   major key gives, is labeled by its quality, "augmented". The key-independent qualities the minor vocabulary
   uses (a triad that is major, minor or diminished on any degree) are labeled "major", "minor", "diminished". */
const char *common_practice_triad_label(CommonPracticeTriad triad);

/*
 * Shared Rule/Constraint for this triad, lazily constructed on first
 * call and owned by this module (callers must not destroy them). Return
 * NULL only if one-time construction failed (allocation failure).
 */
const Rule *common_practice_triad_rule(CommonPracticeTriad triad);
const Constraint *common_practice_triad_constraint(CommonPracticeTriad triad);

/*
 * Builds a real, populated Chord claiming that the Sonority named by
 * sonority_target_id is this triad under common-practice-minimal-v1.
 * Takes ownership of sonority_target_id on success (destroyed
 * transitively by chord_destroy(*out)); on failure the caller still
 * owns sonority_target_id.
 */
ChordResult common_practice_build_triad_chord(CommonPracticeTriad triad, EntityID *sonority_target_id, Chord **out);

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
