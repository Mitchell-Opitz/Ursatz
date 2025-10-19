#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_NEIGHBOR_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_NEIGHBOR_RULES_H

#include <stdbool.h>

#include "harmony_six_four.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Neighbor 6/4 rules for standard-harmony-v1, on a HarmonySixFour (harmony_six_four.h): a
 * 6/4 chord over a static bass in which two upper voices make upper-neighbor motion,
 * sometimes called the pedal 6/4 for the static pedal in the bass. It is most commonly found
 * prolonging I or V: I IV6/4 I over do do do, V I6/4 V over sol sol sol. The chords on both
 * sides are the same and in root position.
 *
 * To write one: write the whole bass, the same note three times (typically do or sol); spell
 * the 6/4 over the middle bass note, with the notes a sixth and a fourth above it and one
 * voice doubling the bass; write the whole soprano as either an upper-neighbor line or the
 * static line (more often static here than with the cadential or passing 6/4); and fill in
 * the inner voices, moving them as little as possible.
 *
 * Hard:
 *
 *   IS_SIX_FOUR        the middle chord is a triad in second inversion with its root a
 *                      fourth above its bass
 *   STATIC_BASS        the bass is the same note in all three chords
 *   SAME_OUTER_CHORDS  the chords on both sides are the same chord, in root position
 *   BASS_IS_ROOT       the static bass is the root of the outer chords, so the 6/4 is a
 *                      fourth above it (IV6/4 over do, I6/4 over sol)
 *   SIX_FOUR_NOTES     the 6/4 is the bass doubled once, and the fourth and the sixth above
 *                      it once each
 *   UPPER_NEIGHBORS    two upper voices go up a step into the 6/4 and back to the same note
 *                      (the upper neighbor), and the third upper voice stays on its note
 *
 * Soft ("most commonly"):
 *
 *   PROLONGS_I_OR_V    the outer chords are I or V
 *
 * Every check returns false for an invalid subject.
 */

typedef enum {
    HARMONY_NEIGHBOR_IS_SIX_FOUR = 0,
    HARMONY_NEIGHBOR_STATIC_BASS,
    HARMONY_NEIGHBOR_SAME_OUTER_CHORDS,
    HARMONY_NEIGHBOR_BASS_IS_ROOT,
    HARMONY_NEIGHBOR_SIX_FOUR_NOTES,
    HARMONY_NEIGHBOR_UPPER_NEIGHBORS,
    HARMONY_NEIGHBOR_PROLONGS_I_OR_V,
    HARMONY_NEIGHBOR_RULE_COUNT
} HarmonyNeighborRule;

const Rule *harmony_neighbor_rule(HarmonyNeighborRule which);
const Constraint *harmony_neighbor_constraint(HarmonyNeighborRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_neighbor_preference(HarmonyNeighborRule which);

/* "is-six-four" "static-bass" "same-outer-chords" "bass-is-root" "six-four-notes"
   "upper-neighbors" "prolongs-i-or-v"; NULL for an invalid rule. */
const char *harmony_neighbor_rule_name(HarmonyNeighborRule which);

/* True when the neighbor 6/4 satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_neighbor_rule_holds(HarmonyNeighborRule which, const HarmonySixFour *six_four);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails, soft_violations for each soft rule. False (both untouched) for an invalid
   subject. */
bool harmony_neighbor_check(const HarmonySixFour *six_four, unsigned *hard_violations, unsigned *soft_violations);

#endif
