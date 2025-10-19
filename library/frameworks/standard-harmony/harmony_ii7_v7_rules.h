#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_II7_V7_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_II7_V7_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * ii7 to V7 for standard-harmony-v1 (Predominant Seventh Chords). When root position ii7
 * resolves to root position V7, either ii7 or V7 needs to be incomplete to avoid a
 * voice-leading problem: ii7 is incomplete when its fifth (la) is omitted and its root (re)
 * doubled, and V7 is incomplete when its fifth (re) is omitted and its root (sol) doubled
 * (harmony_seventh_rules.h). The subject is the two chords and their notes as diatonic steps for
 * each voice part from the soprano to the bass (harmony_steps.h; do is 0, re is 1, an octave is
 * 7).
 *
 * Hard:
 *
 *   PROGRESSION     the chords are ii7 and V7, both in root position
 *   CHORD_NOTES     each chord has only the notes of its chord, with its root, third, and
 *                   seventh present and its root in the bass
 *   ONE_INCOMPLETE  ii7 is incomplete (re twice, no la) or V7 is incomplete (sol twice, no re),
 *                   or both
 *   NO_PARALLELS    no two voices make parallel fifths or octaves (a unison counts as an
 *                   octave)
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[2];
    int steps[2][VOICE_PART_COUNT];
} HarmonyIi7V7;

typedef enum {
    HARMONY_II7_V7_PROGRESSION = 0,
    HARMONY_II7_V7_CHORD_NOTES,
    HARMONY_II7_V7_ONE_INCOMPLETE,
    HARMONY_II7_V7_NO_PARALLELS,
    HARMONY_II7_V7_RULE_COUNT
} HarmonyIi7V7Rule;

/* True when the two chords are ii7 then V7, both seventh chords in root position, and neither
   marked cadential. False for any other chords, an invalid chord, and NULL. */
bool harmony_ii7_v7_progression_is(const HarmonyChord chords[2]);

/* True for a valid mode and two valid chords. */
bool harmony_ii7_v7_valid(const HarmonyIi7V7 *subject);

const Rule *harmony_ii7_v7_rule(HarmonyIi7V7Rule which);
const Constraint *harmony_ii7_v7_constraint(HarmonyIi7V7Rule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_ii7_v7_preference(HarmonyIi7V7Rule which);

/* "progression" "chord-notes" "one-incomplete" "no-parallels"; NULL for an invalid rule. */
const char *harmony_ii7_v7_rule_name(HarmonyIi7V7Rule which);

/* True when the subject satisfies the rule. False when it does not, and for an invalid subject
   or an invalid rule. */
bool harmony_ii7_v7_rule_holds(HarmonyIi7V7Rule which, const HarmonyIi7V7 *subject);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails.
   soft_violations is always written as 0, since every rule here is hard. False (both
   untouched) for an invalid subject. */
bool harmony_ii7_v7_check(const HarmonyIi7V7 *subject, unsigned *hard_violations, unsigned *soft_violations);

#endif
