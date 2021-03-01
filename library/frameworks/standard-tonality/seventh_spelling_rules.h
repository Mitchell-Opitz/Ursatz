#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_SPELLING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SEVENTH_SPELLING_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "seventh_rules.h"

/*
 * Seventh chord spelling rules for standard-tonality-v1: building a
 * seventh chord from its root and quality (what a chord symbol gives).
 * The steps:
 *
 *   KEY_SIGNATURE  draw the root, draw the notes a generic third, fifth,
 *                  and seventh above it, and take their accidentals from
 *                  the major key signature of the root: a major-major
 *                  seventh chord's notes are the notes of the root's
 *                  major scale (D major-major: D F-sharp A C-sharp)
 *   ALTERATION     any other quality is the major-major seventh chord
 *                  with its third, fifth, and/or seventh altered:
 *                  major-minor lowers the seventh a half step, minor-
 *                  minor lowers the third and the seventh, half-
 *                  diminished lowers the third, fifth, and seventh, and
 *                  fully diminished lowers the third and fifth a half
 *                  step and the seventh two
 *
 * A-flat fully diminished, for example: A-flat C E-flat G is the
 * major-major seventh chord (A-flat's key has E-flat; G is its major
 * seventh); lowering the third and fifth gives A-flat C-flat E-double-
 * flat, and lowering the seventh twice gives G-double-flat.
 *
 * A seventh chord is four PitchSpellings, root first. The result agrees
 * with the interval rules: its triad and seventh are the qualities
 * standard_tonality_seventh_parts gives.
 */

typedef enum {
    STANDARD_TONALITY_SEVENTH_SPELLING_RULE_KEY_SIGNATURE = 0,
    STANDARD_TONALITY_SEVENTH_SPELLING_RULE_ALTERATION,
    STANDARD_TONALITY_SEVENTH_SPELLING_RULE_COUNT
} StandardTonalitySeventhSpellingRule;

const Rule *standard_tonality_seventh_spelling_rule(StandardTonalitySeventhSpellingRule which);
const Constraint *standard_tonality_seventh_spelling_constraint(StandardTonalitySeventhSpellingRule which);

/* ALTERATION: half steps the third, the fifth, and the seventh move from
   the major-major seventh chord's: major-major 0 0 0, major-minor 0 0 -1,
   minor-minor -1 0 -1, half-diminished -1 -1 -1, fully diminished -1 -1
   -2. False (outputs untouched) for an invalid quality or NULL outputs. */
bool standard_tonality_seventh_alteration(StandardTonalitySeventhQuality quality, int *third_shift, int *fifth_shift,
                                          int *seventh_shift);

/*
 * Spells the seventh chord of the quality on `root` into out[0..3] (root,
 * third, fifth, seventh) by the interval rules; the caller provides 4
 * pointers and destroys each with pitch_spelling_destroy. out[0] is a
 * copy of root. False, leaving out untouched, for NULL, an invalid
 * quality, allocation failure, or a note needing more than a double
 * accidental.
 */
bool standard_tonality_spell_seventh(const PitchSpelling *root, StandardTonalitySeventhQuality quality,
                                     PitchSpelling *out[4]);

/*
 * KEY_SIGNATURE and ALTERATION as the steps say: the major-major
 * seventh chord's notes from the root's major scale, then altered. Same
 * result as standard_tonality_spell_seventh wherever both exist; false
 * (also) when the root has no major scale (its key signature is
 * imaginary).
 */
bool standard_tonality_spell_seventh_by_key_signature(const PitchSpelling *root,
                                                      StandardTonalitySeventhQuality quality, PitchSpelling *out[4]);

#endif
