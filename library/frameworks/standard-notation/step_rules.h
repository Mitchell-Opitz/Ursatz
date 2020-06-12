#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_STEP_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_STEP_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Half step and whole step rules for standard-notation-v1 (12-TET
 * semitone numbering, see semitone.h):
 *
 *   HALF_STEP           the smallest interval in Western notation: one
 *                       semitone, the adjacent key on the keyboard
 *   WHOLE_STEP          two half steps: two keys away
 *   NATURAL_HALF_STEPS  of the white-key neighbors only E-F and B-C are
 *                       half steps (no black key between them); every
 *                       other adjacent pair of letters is a whole step
 */

typedef enum {
    STANDARD_NOTATION_STEP_RULE_HALF_STEP = 0,
    STANDARD_NOTATION_STEP_RULE_WHOLE_STEP,
    STANDARD_NOTATION_STEP_RULE_NATURAL_HALF_STEPS,
    STANDARD_NOTATION_STEP_RULE_COUNT
} StandardNotationStepRule;

const Rule *standard_notation_step_rule(StandardNotationStepRule which);
const Constraint *standard_notation_step_constraint(StandardNotationStepRule which);

/* HALF_STEP / WHOLE_STEP: semitone distance of exactly 1 / 2 in either
   direction. False for NULL. */
bool standard_notation_is_half_step(const PitchSpelling *a, const PitchSpelling *b);
bool standard_notation_is_whole_step(const PitchSpelling *a, const PitchSpelling *b);

/* WHOLE_STEP: half steps in a whole step (2). */
int standard_notation_half_steps_per_whole_step(void);

/* The semitone number half_steps above spelling (below when negative).
   A whole step is two half steps. */
int standard_notation_semitone_above(const PitchSpelling *spelling, int half_steps);

/* The letter after `letter` in the loop: C D E F G A B C. */
PitchLetter standard_notation_next_letter(PitchLetter letter);

/* NATURAL_HALF_STEPS: semitones from letter up to the next letter (1 for
   E and B, 2 for the rest). */
int standard_notation_natural_step_size(PitchLetter letter);

/* NATURAL_HALF_STEPS: true for E and B, whose next letter is a half step away. */
bool standard_notation_natural_half_step_above(PitchLetter letter);

#endif
