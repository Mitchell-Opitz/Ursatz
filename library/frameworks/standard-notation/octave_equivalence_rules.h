#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_OCTAVE_EQUIVALENCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_OCTAVE_EQUIVALENCE_RULES_H

#include <stdbool.h>

#include "pitch/clef.h"
#include "pitch/pitch_spelling.h"
#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Octave equivalence rule for standard-notation-v1:
 *
 *   OCTAVE_EQUIVALENCE  pitches are named by the letters A-G, which repeat
 *                       in a loop; pitches an octave apart share a letter
 *
 * This is an assumption of THIS framework, not of music in general (some
 * ancient Greek theorists named pitches with more than seven letters).
 * It is stated here as a framework rule, rather than hard-coded into
 * src/pitch/, so a framework for a different system can assert something
 * else -- src/pitch/ deliberately defines no default equivalence relation
 * (docs/modules/pitch.md, PitchClass).
 */

typedef enum {
    STANDARD_NOTATION_OCTAVE_RULE_OCTAVE_EQUIVALENCE = 0,
    STANDARD_NOTATION_OCTAVE_RULE_COUNT
} StandardNotationOctaveRule;

const Rule *standard_notation_octave_rule(StandardNotationOctaveRule which);
const Constraint *standard_notation_octave_constraint(StandardNotationOctaveRule which);

/* Letters in the loop: 7. */
int standard_notation_letter_count(void);

/* Octave equivalence: same letter, any octaves. */
bool standard_notation_octave_equivalent(PitchLetter a, PitchLetter b);

/*
 * True when the pitches written at the two positions under clef are
 * octave-equivalent. False for an unpitched clef or NULL argument.
 */
bool standard_notation_positions_octave_equivalent(const Clef *clef, const StaffPosition *a,
                                                   const StaffPosition *b);

#endif
