#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PASSING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PASSING_RULES_H

#include <stdbool.h>

#include "harmony_six_four.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Passing 6/4 rules for standard-harmony-v1, on a HarmonySixFour (harmony_six_four.h): a 6/4
 * chord built on a passing tone in the bass, which most commonly prolongs a tonic or a
 * predominant harmony, as in I V6/4 I6 or the same bass between predominants. The chords on
 * both sides always have the same function, and it works backward (I6 V6/4 I) too.
 *
 * To write one: write the whole bass, three notes in stepwise motion whose first and last
 * belong to the same functional area (tonic or predominant), the middle note the passing
 * tone; spell the 6/4 as for the cadential 6/4, with the notes a fourth and a sixth above
 * the bass and one voice doubling the bass; write the whole soprano as a line that moves by
 * step, not the static line; and fill in the inner voices, moving them as little as possible.
 *
 * Hard:
 *
 *   IS_SIX_FOUR      the middle chord is a triad in second inversion with its root a fourth
 *                    above its bass
 *   STEPWISE_BASS    the bass goes by step three times in one direction (up or down)
 *   SAME_FUNCTION    the chords on both sides have the same function
 *   SIX_FOUR_NOTES   the 6/4 is the bass doubled once, and the fourth and the sixth above it
 *                    once each
 *
 * Soft ("most commonly", "should", "as little as possible"):
 *
 *   PROLONGS_TONIC_OR_PREDOMINANT
 *                    the function on both sides is tonic or predominant
 *   SOPRANO_BY_STEP  the soprano moves by step into the 6/4 and out of it, not the static line
 *   INNER_VOICES_MOVE_LITTLE
 *                    the alto and the tenor each stay or move by step
 *
 * Every check returns false for an invalid subject.
 */

typedef enum {
    HARMONY_PASSING_IS_SIX_FOUR = 0,
    HARMONY_PASSING_STEPWISE_BASS,
    HARMONY_PASSING_SAME_FUNCTION,
    HARMONY_PASSING_SIX_FOUR_NOTES,
    HARMONY_PASSING_PROLONGS_TONIC_OR_PREDOMINANT,
    HARMONY_PASSING_SOPRANO_BY_STEP,
    HARMONY_PASSING_INNER_VOICES_MOVE_LITTLE,
    HARMONY_PASSING_RULE_COUNT
} HarmonyPassingRule;

const Rule *harmony_passing_rule(HarmonyPassingRule which);
const Constraint *harmony_passing_constraint(HarmonyPassingRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_passing_preference(HarmonyPassingRule which);

/* "is-six-four" "stepwise-bass" "same-function" "six-four-notes"
   "prolongs-tonic-or-predominant" "soprano-by-step" "inner-voices-move-little"; NULL for an
   invalid rule. */
const char *harmony_passing_rule_name(HarmonyPassingRule which);

/* True when the passing 6/4 satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_passing_rule_holds(HarmonyPassingRule which, const HarmonySixFour *six_four);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails, soft_violations for each soft rule. False (both untouched) for an invalid
   subject. */
bool harmony_passing_check(const HarmonySixFour *six_four, unsigned *hard_violations, unsigned *soft_violations);

#endif
