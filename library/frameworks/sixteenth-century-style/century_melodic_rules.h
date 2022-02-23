#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_MELODIC_RULES_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_MELODIC_RULES_H

#include <stdbool.h>

#include "century_score.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Melodic rules for sixteenth-century-style-v1, applied to every voice of a
 * score on its notes (a tie does not make a new note). Conjunct motion
 * predominates and large leaps are recovered; sevenths and tritones are
 * avoided altogether.
 *
 * Hard:
 *
 *   NO_DISSONANT_LEAPS     no melodic interval is a seventh, or augmented or
 *                          diminished (the tritone included)
 *   NO_OUTLINED_DISSONANCE no seventh, augmented, or diminished interval is
 *                          outlined by the boundaries of a gesture: the
 *                          first and last notes of a run in one direction
 *   RANGE_LIMIT            no voice spans more than a twelfth
 *
 * Soft:
 *
 *   SIXTHS                 the only sixth is the rising minor sixth; rising
 *                          major sixths are rare and descending sixths
 *                          extremely rare
 *   LEAP_ORDER             of two successive leaps in the same direction,
 *                          the larger is the lower one (the first when
 *                          rising, the second when descending)
 *   LEAP_RECOVERY          a leap larger than a fourth is followed by
 *                          motion in the opposite direction
 *   NO_TRIAD_OUTLINE       no two successive thirds in the same direction,
 *                          which would arpeggiate a triad
 *   ARCH                   a voice of five notes or more does not begin or
 *                          end on its highest note
 *   RANGE_PREFERRED        no voice spans more than 14 semitones
 *   FINAL_STEP             a voice approaches its last note by step (or
 *                          repeats it)
 *   LEADING_NOTE           a voice that ends on the tonic from a step below
 *                          raises that note to a half step, unless the mode
 *                          is Phrygian; holds when the tonic is NULL
 *
 * Every check returns false for an invalid score.
 */

#define SIXTEENTH_CENTURY_MAX_RANGE_STEPS 11
#define SIXTEENTH_CENTURY_PREFERRED_RANGE_SEMITONES 14
#define SIXTEENTH_CENTURY_ARCH_MIN_NOTES 5

typedef enum {
    SIXTEENTH_CENTURY_MELODIC_NO_DISSONANT_LEAPS = 0,
    SIXTEENTH_CENTURY_MELODIC_NO_OUTLINED_DISSONANCE,
    SIXTEENTH_CENTURY_MELODIC_RANGE_LIMIT,
    SIXTEENTH_CENTURY_MELODIC_SIXTHS,
    SIXTEENTH_CENTURY_MELODIC_LEAP_ORDER,
    SIXTEENTH_CENTURY_MELODIC_LEAP_RECOVERY,
    SIXTEENTH_CENTURY_MELODIC_NO_TRIAD_OUTLINE,
    SIXTEENTH_CENTURY_MELODIC_ARCH,
    SIXTEENTH_CENTURY_MELODIC_RANGE_PREFERRED,
    SIXTEENTH_CENTURY_MELODIC_FINAL_STEP,
    SIXTEENTH_CENTURY_MELODIC_LEADING_NOTE,
    SIXTEENTH_CENTURY_MELODIC_RULE_COUNT
} SixteenthCenturyMelodicRule;

const Rule *sixteenth_century_melodic_rule(SixteenthCenturyMelodicRule which);
const Constraint *sixteenth_century_melodic_constraint(SixteenthCenturyMelodicRule which);

/* "no-dissonant-leaps" "no-outlined-dissonance" "range-limit" "sixths" "leap-order"
   "leap-recovery" "no-triad-outline" "arch" "range-preferred" "final-step"
   "leading-note"; NULL for an invalid rule. */
const char *sixteenth_century_melodic_rule_name(SixteenthCenturyMelodicRule which);

/* True when the score satisfies the rule in every voice. False when it does
   not, and for an invalid score or an invalid rule. */
bool sixteenth_century_melodic_rule_holds(SixteenthCenturyMelodicRule which, const SixteenthCenturyScore *score);

/* The rules the score breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid score. */
bool sixteenth_century_melodic_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                     unsigned *soft_violations);

#endif
