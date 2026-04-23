#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_SEVENTH_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_CHORD_VOCABULARY_SEVENTH_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Seventh-chord vocabulary for common-practice-minimal-v1: the four
 * seventh-chord qualities that occur diatonically within a major or
 * natural-minor key -- dominant7, major7, minor7, half-diminished7 --
 * and the fully-diminished7 of harmonic minor (the raised leading tone's
 * seventh). Deliberately excludes chromatic/secondary-dominant
 * sevenths and any quality resolution that would need a scale degree
 * -- see seventh_identification.h for why matching is by root-relative
 * interval content rather than scale degree.
 *
 * Each quality has one Rule and one Constraint (CONSTRAINT_TYPE_HARD)
 * referencing the shared common_practice_minimal_framework(), same
 * shape as chord_vocabulary.h's triads. This module never calls
 * constraint_evaluate (see chord_vocabulary.h for that boundary note).
 */

typedef enum {
    COMMON_PRACTICE_SEVENTH_DOMINANT7 = 0,
    COMMON_PRACTICE_SEVENTH_MAJOR7,
    COMMON_PRACTICE_SEVENTH_MINOR7,
    COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7,
    COMMON_PRACTICE_SEVENTH_DIMINISHED7,
    COMMON_PRACTICE_SEVENTH_COUNT
} CommonPracticeSeventh;

/* The first four sevenths are the diatonic set of a major key: the subset matcher (seventh_identification.h)
   scans only these. The fully-diminished seventh is the harmonic-minor vii-dim-7. */
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
 * Shared Rule/Constraint for this seventh-chord quality, lazily
 * constructed on first call and owned by this module (callers must not
 * destroy them). Return NULL only if one-time construction failed
 * (allocation failure).
 */
const Rule *common_practice_seventh_rule(CommonPracticeSeventh seventh);
const Constraint *common_practice_seventh_constraint(CommonPracticeSeventh seventh);

/*
 * Builds a real, populated Chord claiming that the Sonority named by
 * sonority_target_id is this seventh-chord quality under
 * common-practice-minimal-v1. Takes ownership of sonority_target_id on
 * success (destroyed transitively by chord_destroy(*out)); on failure
 * the caller still owns sonority_target_id.
 */
ChordResult common_practice_build_seventh_chord(CommonPracticeSeventh seventh, EntityID *sonority_target_id,
                                                 Chord **out);

/*
 * As common_practice_build_seventh_chord, but the Interpretation's scalar confidence is
 * confidence_numerator / confidence_denominator (a rational in [0, 1]) instead of 1, and the
 * Interpretation's id has interpretation_id_suffix (NULL for none) appended, so two Chords over one
 * Sonority (a best and a runner-up) get distinct ids. The existing builder is this one with confidence
 * 1/1 and no suffix. Takes ownership of sonority_target_id on success only.
 */
ChordResult common_practice_build_seventh_chord_with_confidence(CommonPracticeSeventh seventh, EntityID *sonority_target_id,
                                                          unsigned long confidence_numerator,
                                                          unsigned long confidence_denominator,
                                                          const char *interpretation_id_suffix, Chord **out);

#endif
