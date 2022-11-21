#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_APPLIED_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_APPLIED_RULES_H

#include <stdbool.h>

#include "harmony_applied.h"
#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Applied chord resolution rules for standard-harmony-v1 (Tonicization): an applied chord
 * (harmony_applied.h) and the chord after it, in four voices. The subject is the applied chord,
 * the chord after it, and the notes of both chords from the soprano to the bass. Tonicization is
 * chromatic, so a note is a diatonic step from the tonic (harmony_steps.h; do is 0, re is 1, an
 * octave is 7) and an alteration, -1 for flat, 0 for natural, 1 for sharp: in C major, the F
 * sharp of V7/V is step 3 raised, and the E flat of vii diminished 7/V is step 2 lowered.
 *
 * The applied chord's leading tone, the secondary leading tone (the third of a secondary
 * dominant, the root of a secondary leading-tone chord), and its chordal seventh, the secondary
 * seventh, resolve just as they normally would in their own key: the leading tone up to the
 * target's root, the seventh down by step.
 *
 * Hard:
 *
 *   PROGRESSION           the applied chord is valid for the mode and the chord after it is the
 *                         target (a triad or a seventh chord, in any inversion), or, when the
 *                         target is V, the cadential 6/4 that embellishes it
 *   CHORD_NOTES           each of the four voices is a note of the applied chord, with its root,
 *                         its third, and, in a seventh chord, its seventh present
 *   LEADING_TONE_RESOLVES the secondary leading tone goes up a semitone. The exception: when the
 *                         chord after is itself a seventh chord, it may go down a semitone, into
 *                         that chord's seventh (D7 G7 C: F sharp, F natural, E)
 *   SEVENTH_RESOLVES      the secondary seventh, in a seventh chord, goes down by step (a
 *                         semitone or a whole tone)
 *   NO_PARALLELS          no two voices make parallel fifths or octaves (a unison counts as an
 *                         octave) from the applied chord to the one after
 *
 * Soft ("rare", "nearly always"):
 *
 *   AVOID_HALF_DIMINISHED the applied chord is not a half-diminished seventh, which is rare in
 *                         classical music
 *   CHROMATIC             at least one note of the applied chord is altered: applied chords
 *                         nearly always involve accidentals
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyAppliedChord applied;
    HarmonyChord next;
    int steps[2][VOICE_PART_COUNT];
    int alter[2][VOICE_PART_COUNT];
} HarmonyAppliedMotion;

typedef enum {
    HARMONY_APPLIED_PROGRESSION = 0,
    HARMONY_APPLIED_CHORD_NOTES,
    HARMONY_APPLIED_LEADING_TONE_RESOLVES,
    HARMONY_APPLIED_SEVENTH_RESOLVES,
    HARMONY_APPLIED_NO_PARALLELS,
    HARMONY_APPLIED_AVOID_HALF_DIMINISHED,
    HARMONY_APPLIED_CHROMATIC,
    HARMONY_APPLIED_RULE_COUNT
} HarmonyAppliedRule;

/* True for a valid mode, a valid applied chord for it, and a valid chord after. */
bool harmony_applied_motion_valid(const HarmonyAppliedMotion *motion);

/* The pitch of a voice in semitones above the tonic's pitch: the step's diatonic semitones in
   the mode (octaves kept) plus its alteration. */
int harmony_applied_pitch(HarmonyMode mode, int step, int alter);

const Rule *harmony_applied_rule(HarmonyAppliedRule which);
const Constraint *harmony_applied_constraint(HarmonyAppliedRule which);

/* "progression" "chord-notes" "leading-tone-resolves" "seventh-resolves" "no-parallels"
   "avoid-half-diminished" "chromatic"; NULL for an invalid rule. */
const char *harmony_applied_rule_name(HarmonyAppliedRule which);

/* True when the motion satisfies the rule. False when it does not, and for an invalid subject
   or an invalid rule. */
bool harmony_applied_rule_holds(HarmonyAppliedRule which, const HarmonyAppliedMotion *motion);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that fails,
   soft_violations for each soft rule. False (both untouched) for an invalid subject. */
bool harmony_applied_check(const HarmonyAppliedMotion *motion, unsigned *hard_violations,
                           unsigned *soft_violations);

#endif
