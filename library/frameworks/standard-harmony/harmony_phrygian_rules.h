#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRYGIAN_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PHRYGIAN_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Phrygian half cadence writing rules for standard-harmony-v1: iv6 to V in minor
 * (harmony_phrygian.h) in four voices. The subject is the two chords and their notes as
 * diatonic steps for each voice part from the soprano to the bass (harmony_steps.h; do is 0,
 * re is 1, an octave is 7). The progression carries a danger of parallels and of an augmented
 * second between le and ti, both avoided by doubling do in the iv6 chord.
 *
 * Hard:
 *
 *   PROGRESSION          the key is minor and the chords are iv6, then V (or V7)
 *   CHORD_NOTES          each chord has only the notes of its chord (V7 also has its
 *                        seventh), with its root and third present and the bass on the third
 *                        in iv6 (le) and on the root in V (sol)
 *   DOUBLED_DO           do is in two voices of the iv6
 *   NO_AUGMENTED_SECOND  no voice goes from le up a step to ti
 *   NO_PARALLELS         no two voices make parallel fifths or octaves (a unison counts as an
 *                        octave)
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[2];
    int steps[2][VOICE_PART_COUNT];
} HarmonyPhrygian;

typedef enum {
    HARMONY_PHRYGIAN_PROGRESSION = 0,
    HARMONY_PHRYGIAN_CHORD_NOTES,
    HARMONY_PHRYGIAN_DOUBLED_DO,
    HARMONY_PHRYGIAN_NO_AUGMENTED_SECOND,
    HARMONY_PHRYGIAN_NO_PARALLELS,
    HARMONY_PHRYGIAN_RULE_COUNT
} HarmonyPhrygianRule;

/* True for a valid mode and two valid chords. */
bool harmony_phrygian_valid(const HarmonyPhrygian *phrygian);

const Rule *harmony_phrygian_rule(HarmonyPhrygianRule which);
const Constraint *harmony_phrygian_constraint(HarmonyPhrygianRule which);

/* "progression" "chord-notes" "doubled-do" "no-augmented-second" "no-parallels"; NULL for an
   invalid rule. */
const char *harmony_phrygian_rule_name(HarmonyPhrygianRule which);

/* True when the cadence satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_phrygian_rule_holds(HarmonyPhrygianRule which, const HarmonyPhrygian *phrygian);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails.
   soft_violations is always written as 0, since every rule here is hard. False (both
   untouched) for an invalid subject. */
bool harmony_phrygian_check(const HarmonyPhrygian *phrygian, unsigned *hard_violations, unsigned *soft_violations);

#endif
