#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_MEDIANT_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_MEDIANT_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmony_mediant.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Mediant writing rules for standard-harmony-v1: I, iii, then a strong predominant or V
 * (harmony_mediant.h) in four voices. The subject is the three chords and their notes as
 * diatonic steps for each voice part from the soprano to the bass (harmony_steps.h; do is 0, re
 * is 1, an octave is 7). The most common use of iii is as a weak predominant that moves through
 * a strong predominant on its way to V: the leading tone, which iii harmonizes as a descending
 * ti (do ti la), descends to la when iii moves to the predominant. It can also go directly to V,
 * where that ti stays as a common tone.
 *
 * Hard:
 *
 *   PROGRESSION               the chords are I, iii, then a strong predominant or V
 *   BASS_LINE                 each chord has its own bass: the root, or the third when the
 *                             chord is inverted (so in root position the bass goes do mi, then
 *                             fa or re, or sol directly to V)
 *   CHORD_NOTES               each chord has only the notes of its chord, with its root and
 *                             third present
 *   DESCENDING_LEADING_TONE   an upper voice is on ti over iii, and each one is do over the I
 *                             and goes down to la over the strong predominant, or stays on ti
 *                             into V
 *   NO_PARALLELS              no two voices make parallel fifths or octaves (a unison counts
 *                             as an octave) from one chord to the next
 *
 * Soft ("usually", "not very common"):
 *
 *   ROOT_POSITION             iii is in root position
 *   THROUGH_PREDOMINANT       iii moves through a strong predominant, not directly to V
 *   AVOID_V_IN_MINOR          the key is major, or iii goes through a predominant: iii directly
 *                             to V does not work well in minor, where III has te and V has ti
 *                             (accidentals are not modeled)
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[3];
    int steps[3][VOICE_PART_COUNT];
} HarmonyMediant;

typedef enum {
    HARMONY_MEDIANT_PROGRESSION = 0,
    HARMONY_MEDIANT_BASS_LINE,
    HARMONY_MEDIANT_CHORD_NOTES,
    HARMONY_MEDIANT_DESCENDING_LEADING_TONE,
    HARMONY_MEDIANT_NO_PARALLELS,
    HARMONY_MEDIANT_ROOT_POSITION,
    HARMONY_MEDIANT_THROUGH_PREDOMINANT,
    HARMONY_MEDIANT_AVOID_V_IN_MINOR,
    HARMONY_MEDIANT_RULE_COUNT
} HarmonyMediantRule;

/* True for a valid mode and three valid chords. */
bool harmony_mediant_valid(const HarmonyMediant *mediant);

const Rule *harmony_mediant_rule(HarmonyMediantRule which);
const Constraint *harmony_mediant_constraint(HarmonyMediantRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_mediant_preference(HarmonyMediantRule which);

/* "progression" "bass-line" "chord-notes" "descending-leading-tone" "no-parallels"
   "root-position" "through-predominant" "avoid-v-in-minor"; NULL for an invalid rule. */
const char *harmony_mediant_rule_name(HarmonyMediantRule which);

/* True when the mediant satisfies the rule. False when it does not, and for an invalid subject
   or an invalid rule. */
bool harmony_mediant_rule_holds(HarmonyMediantRule which, const HarmonyMediant *mediant);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails,
   soft_violations for each soft rule. False (both untouched) for an invalid subject. */
bool harmony_mediant_check(const HarmonyMediant *mediant, unsigned *hard_violations, unsigned *soft_violations);

#endif
