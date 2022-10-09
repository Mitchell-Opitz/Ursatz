#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PLAGAL_SIX_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PLAGAL_SIX_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Plagal (IV6) prolongation for standard-harmony-v1 (La in the Bass at Beginnings, Middles,
 * and Endings). At a phrase beginning, la in the bass can prolong tonic by way of a plagal
 * (IV6), which is not a predominant here and is written in parentheses: I (IV6) I6, three
 * chords with the bass do la mi. Writing it is relatively easy: the bass always arpeggiates
 * down, the most common soprano is mi fa sol, and the other voices move by step or common
 * tone.
 *
 * The subject is the three chords and their notes as diatonic steps for each voice part from
 * the soprano to the bass (harmony_steps.h; do is 0, re is 1, an octave is 7).
 *
 * Hard:
 *
 *   PROGRESSION          the chords are I (a triad, root position), IV6, and I6
 *   BASS_LINE            the bass is do la mi, and each move is a leap down
 *   CHORD_NOTES          each chord has only the notes of its chord, with its root and third
 *                        present and the bass on the root in I and on the third in IV6 and I6
 *   STEP_OR_COMMON_TONE  each upper voice moves by step or stays on its note
 *   NO_PARALLELS         no two voices make parallel fifths or octaves (a unison counts as an
 *                        octave)
 *
 * Soft ("most common"):
 *
 *   SOPRANO_MI_FA_SOL    the soprano is mi fa sol
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[3];
    int steps[3][VOICE_PART_COUNT];
} HarmonyPlagalSix;

typedef enum {
    HARMONY_PLAGAL_SIX_PROGRESSION = 0,
    HARMONY_PLAGAL_SIX_BASS_LINE,
    HARMONY_PLAGAL_SIX_CHORD_NOTES,
    HARMONY_PLAGAL_SIX_STEP_OR_COMMON_TONE,
    HARMONY_PLAGAL_SIX_NO_PARALLELS,
    HARMONY_PLAGAL_SIX_SOPRANO_MI_FA_SOL,
    HARMONY_PLAGAL_SIX_RULE_COUNT
} HarmonyPlagalSixRule;

/* True when the three chords are I (a triad, root position), IV6, and I6, all triads and none
   cadential. False for any other chords, an invalid chord, and NULL. */
bool harmony_plagal_six_progression_is(const HarmonyChord chords[3]);

/* True for a valid mode and three valid chords. */
bool harmony_plagal_six_valid(const HarmonyPlagalSix *plagal);

const Rule *harmony_plagal_six_rule(HarmonyPlagalSixRule which);
const Constraint *harmony_plagal_six_constraint(HarmonyPlagalSixRule which);

/* "progression" "bass-line" "chord-notes" "step-or-common-tone" "no-parallels"
   "soprano-mi-fa-sol"; NULL for an invalid rule. */
const char *harmony_plagal_six_rule_name(HarmonyPlagalSixRule which);

/* True when the prolongation satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_plagal_six_rule_holds(HarmonyPlagalSixRule which, const HarmonyPlagalSix *plagal);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails,
   soft_violations for each soft rule. False (both untouched) for an invalid subject. */
bool harmony_plagal_six_check(const HarmonyPlagalSix *plagal, unsigned *hard_violations, unsigned *soft_violations);

#endif
