#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PLAGAL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PLAGAL_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmony_plagal.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Plagal motion writing rules for standard-harmony-v1: (IV)-I (harmony_plagal.h) written
 * in four voices. The subject is the two chords, their notes as diatonic steps for each
 * voice part from the soprano to the bass (harmony_steps.h; do is 0, re is 1, an octave is
 * 7), and where the motion sits, which the caller gives.
 *
 * To write it after an authentic cadence: make both I and (IV) complete (do not omit the
 * fifth), double the bass, and move all the upper voices by step or common tone, with do in
 * the soprano. At a phrase beginning the root position version is the same; (IV) may also
 * go to I6, where the danger is parallels between (IV) and I6.
 *
 * Hard:
 *
 *   PROGRESSION         the two chords are plagal motion in the placement: IV, then I, or
 *                       I6 at a phrase beginning only
 *   CHORD_NOTES         each chord has only the notes of its chord, all three (the root,
 *                       the third, and the fifth) present, with the bass on its root (or,
 *                       in I6, its third). A chord in root position doubles its bass
 *   STEP_OR_COMMON_TONE with both chords in root position, each upper voice moves by step
 *                       or stays on its note. I6 has no such rule
 *   NO_PARALLELS        no two voices make parallel fifths or octaves (a unison counts as
 *                       an octave) from (IV) to I. In (IV) to I6, an upper voice that
 *                       follows the bass down a step from the bass's own note makes
 *                       parallel octaves with it
 *
 * Soft ("most common", "more common by far"):
 *
 *   SOPRANO_DO          do is in the soprano of both chords, as the melody has already
 *                       completed its journey to do after the cadence. This always holds
 *                       when the motion is at a phrase beginning
 *   AVOID_AT_BEGINNING  at a phrase beginning, tonic is more commonly prolonged by an
 *                       inverted V7, so (IV) is the less common choice there. This always
 *                       holds after a cadence
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyPlagalPlacement placement;
    HarmonyChord chords[2];
    int steps[2][VOICE_PART_COUNT];
} HarmonyPlagalMotion;

typedef enum {
    HARMONY_PLAGAL_PROGRESSION = 0,
    HARMONY_PLAGAL_CHORD_NOTES,
    HARMONY_PLAGAL_STEP_OR_COMMON_TONE,
    HARMONY_PLAGAL_NO_PARALLELS,
    HARMONY_PLAGAL_SOPRANO_DO,
    HARMONY_PLAGAL_AVOID_AT_BEGINNING,
    HARMONY_PLAGAL_RULE_COUNT
} HarmonyPlagalRule;

/* True for a valid mode, a valid placement, and two valid chords. */
bool harmony_plagal_motion_valid(const HarmonyPlagalMotion *motion);

const Rule *harmony_plagal_rule(HarmonyPlagalRule which);
const Constraint *harmony_plagal_constraint(HarmonyPlagalRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_plagal_preference(HarmonyPlagalRule which);

/* "progression" "chord-notes" "step-or-common-tone" "no-parallels" "soprano-do"
   "avoid-at-beginning"; NULL for an invalid rule. */
const char *harmony_plagal_rule_name(HarmonyPlagalRule which);

/* True when the motion satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_plagal_rule_holds(HarmonyPlagalRule which, const HarmonyPlagalMotion *motion);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails, soft_violations for each soft rule. False (both untouched) for an invalid
   subject. */
bool harmony_plagal_check(const HarmonyPlagalMotion *motion, unsigned *hard_violations, unsigned *soft_violations);

#endif
