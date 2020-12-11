#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_CHROMATIC_SCALE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_CHROMATIC_SCALE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Chromatic scale rules for standard-tonality-v1:
 *
 *   HALF_STEPS         a chromatic scale is twelve adjacent half steps
 *   NATURAL_HALF_STEPS two pairs of white keys are half steps already:
 *                      B to C and E to F
 *   SPELLING           chromatic scales are usually, but not always,
 *                      written with sharps (and naturals) ascending and
 *                      flats (and naturals) descending; a scale that
 *                      begins on a flat note is written the same way,
 *                      with naturals and sharps ascending
 *   SOLFEGE            ascending uses the raised syllables (do di re ri
 *                      mi fa fi sol si la li ti do); descending the
 *                      lowered ones (do ti te la le sol se fa mi me re
 *                      ra do)
 *
 * A chromatic scale is a list of PitchSpelling, thirteen long: the
 * twelve half steps from the first note to the same pitch class an
 * octave away. A descending scale runs from the upper note down. The
 * conventional spelling is the one SPELLING describes, with the first
 * and last note spelled as the tonic is; it is a convention, not the
 * definition, so HALF_STEPS checks only the steps.
 */

#define STANDARD_TONALITY_CHROMATIC_STEP_COUNT 12
#define STANDARD_TONALITY_CHROMATIC_NOTE_COUNT 13

typedef enum {
    STANDARD_TONALITY_CHROMATIC_ASCENDING = 0,
    STANDARD_TONALITY_CHROMATIC_DESCENDING
} StandardTonalityChromaticDirection;

typedef enum {
    STANDARD_TONALITY_CHROMATIC_RULE_HALF_STEPS = 0,
    STANDARD_TONALITY_CHROMATIC_RULE_NATURAL_HALF_STEPS,
    STANDARD_TONALITY_CHROMATIC_RULE_SPELLING,
    STANDARD_TONALITY_CHROMATIC_RULE_SOLFEGE,
    STANDARD_TONALITY_CHROMATIC_RULE_COUNT
} StandardTonalityChromaticRule;

const Rule *standard_tonality_chromatic_rule(StandardTonalityChromaticRule which);
const Constraint *standard_tonality_chromatic_constraint(StandardTonalityChromaticRule which);

/* HALF_STEPS: true when notes (count must be 13) rise by one semitone
   at each step (ascending) or fall by one (descending), whatever their
   spelling. False for NULL, a NULL note, the wrong count, or an invalid
   direction. */
bool standard_tonality_is_chromatic_scale(StandardTonalityChromaticDirection direction,
                                          const PitchSpelling *const *notes, size_t count);

/* NATURAL_HALF_STEPS: true for E and B, whose next letter is a half step
   up (E to F, B to C). */
bool standard_tonality_natural_half_step_above(PitchLetter letter);

/* SPELLING: true when the eleven notes between the first and last are
   spelled by the convention for the direction: natural or sharp
   ascending, natural or flat descending (no E-sharp, B-sharp, F-flat,
   C-flat). The first and last notes (the tonic) are not checked. False
   for NULL, a NULL note, the wrong count, or an invalid direction. */
bool standard_tonality_chromatic_spelling_conventional(StandardTonalityChromaticDirection direction,
                                                       const PitchSpelling *const *notes, size_t count);

/*
 * Builds the conventionally spelled chromatic scale into out[0..12].
 * Ascending, `tonic` is the lowest note and out[12] its octave above;
 * descending, `tonic` is the highest note and out[12] its octave below.
 * The tonic keeps the spelling given (a flat tonic ascends through
 * naturals and sharps all the same). The caller provides 13 pointers
 * and destroys each with pitch_spelling_destroy. False, leaving out
 * untouched, for NULL, an invalid direction, or allocation failure.
 */
bool standard_tonality_build_chromatic_scale(StandardTonalityChromaticDirection direction, const PitchSpelling *tonic,
                                             PitchSpelling *out[STANDARD_TONALITY_CHROMATIC_NOTE_COUNT]);

/* SOLFEGE: the syllable of the index-th note (0..12) of the scale in the
   direction: raised syllables ascending, lowered descending. NULL for an
   invalid direction or index. */
const char *standard_tonality_chromatic_scale_solfege(StandardTonalityChromaticDirection direction, size_t index);

#endif
