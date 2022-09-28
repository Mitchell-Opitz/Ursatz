#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PLAGAL_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PLAGAL_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * Plagal motion for standard-harmony-v1 (Plagal Motion as a Form of Prolongation).
 * Plagal motion is IV to I used to prolong tonic, not to strengthen a phrase's motion
 * toward the dominant, so the IV is not a predominant here; in analysis it is written
 * in parentheses, (IV). It prolongs tonic in two places only:
 *
 *   AFTER_CADENCE     after an authentic cadence, most commonly with both chords in root
 *                     position and do in the soprano, where the melody has already
 *                     completed its journey to do
 *   PHRASE_BEGINNING  at the beginning of a phrase, in place of the more common tonic
 *                     prolongations with dominant-function chords. (IV) may go to I or
 *                     to I6 here
 *
 * It is called plagal motion, not a plagal cadence: a cadence ends a phrase, and in the
 * 18th century (IV)-I is far more often a prolongation. A phrase that really ends on
 * (IV)-I does exist, but it is not modeled (it is not a cadence in this framework, see
 * harmony_cadence_rules.h), and so there is no placement for it. The placement is the
 * caller's: nothing here finds a phrase's beginning or a cadence.
 *
 * A plagal motion is two HarmonyChords (harmony_chord.h): IV is the triad on degree 4
 * in root position, and I is the triad on degree 1 in root position or, at a phrase
 * beginning, in first inversion.
 */

typedef enum {
    HARMONY_PLAGAL_AFTER_CADENCE = 0,
    HARMONY_PLAGAL_PHRASE_BEGINNING,
    HARMONY_PLAGAL_PLACEMENT_COUNT
} HarmonyPlagalPlacement;

/* True for the two placements above; false for any other value. */
bool harmony_plagal_placement_valid(HarmonyPlagalPlacement placement);

/* "after-cadence" "phrase-beginning"; NULL for an invalid placement. */
const char *harmony_plagal_placement_name(HarmonyPlagalPlacement placement);

/* True when the two chords are plagal motion in that placement: IV (a triad, root
   position) then I (a triad, root position, or first inversion at a phrase beginning).
   False for any other pair, for a chord marked cadential or an invalid one, for an
   invalid placement, and for NULL. */
bool harmony_plagal_progression_is(const HarmonyChord chords[2], HarmonyPlagalPlacement placement);

#endif
