#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PROLONGATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_PROLONGATION_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmony_prolongation.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/beat_strength.h"
#include "semantics/voice_part.h"

/*
 * Tonic prolongation writing rules for standard-harmony-v1: a three-chord
 * prolongation of tonic (harmony_prolongation.h) written in four voices. The
 * subject is the three chords, their notes as diatonic steps for each voice part
 * from the soprano to the bass (harmony_steps.h; do is 0, re is 1, ti below do is
 * -1, an octave is 7), and the beat strength of each chord.
 *
 * Hard (structure):
 *
 *   PROGRESSION   the three chords are one of the four progressions: I V6 (or V6/5)
 *                 I, I V4/3 I6, I V4/3 I, or I V4/2 I6
 *   BASS_LINE     the bass is the line of the progression: do ti do, do re mi, do re
 *                 do, or do fa mi (an inverted V has ti, re, or fa in the bass: the
 *                 third of V, the fifth of V7, or the seventh of V7)
 *   CHORD_NOTES   each chord has only the notes of its chord. I and I6: do, mi, sol,
 *                 with do and mi (the root and the third) present. V6: sol, ti, re,
 *                 with sol and ti present. An inverted V7 is four voices on four
 *                 notes: sol, ti, re, and fa, once each
 *
 * Hard (voice leading), as in the V7 resolution (harmony_seventh_rules.h):
 *
 *   RESOLVES            the V resolves into the last chord by the tendencies: ti goes
 *                       up a step to do (so V6 and V6/5, with ti in the bass, resolve
 *                       to root position I), fa goes down a step to mi (so V4/2, with
 *                       fa in the bass, goes to I6), and re goes a step to do or mi
 *                       (so V4/3, with re in the bass, goes to either). The one
 *                       exception is the chordal seventh going up: in I V4/3 I6, an
 *                       upper voice may take the line mi fa sol, fa going up to sol,
 *                       because the bass (do re mi) makes parallel tenths with it all
 *                       the way
 *   MINOR_LEADING_TONE  in minor, V has the leading tone (ti); in major this always
 *                       holds
 *   NO_PARALLELS        no two voices make parallel fifths or octaves (a unison
 *                       counts as an octave) from one chord to the next
 *
 * Every check returns false for an invalid prolongation.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[3];
    int steps[3][VOICE_PART_COUNT];
    BeatStrength beat[3];
    bool dominant_has_leading_tone;
} HarmonyTonicProlongation;

typedef enum {
    HARMONY_PROLONGATION_PROGRESSION = 0,
    HARMONY_PROLONGATION_BASS_LINE,
    HARMONY_PROLONGATION_CHORD_NOTES,
    HARMONY_PROLONGATION_RESOLVES,
    HARMONY_PROLONGATION_MINOR_LEADING_TONE,
    HARMONY_PROLONGATION_NO_PARALLELS,
    HARMONY_PROLONGATION_RULE_COUNT
} HarmonyProlongationRule;

/* True for a valid mode, three valid chords, and a valid beat strength for each. */
bool harmony_prolongation_valid(const HarmonyTonicProlongation *prolongation);

const Rule *harmony_prolongation_rule(HarmonyProlongationRule which);
const Constraint *harmony_prolongation_constraint(HarmonyProlongationRule which);

/* "progression" "bass-line" "chord-notes" "resolves" "minor-leading-tone"
   "no-parallels"; NULL for an invalid rule. */
const char *harmony_prolongation_rule_name(HarmonyProlongationRule which);

/* True when the prolongation satisfies the rule. False when it does not, and for an
   invalid prolongation or an invalid rule. */
bool harmony_prolongation_rule_holds(HarmonyProlongationRule which, const HarmonyTonicProlongation *prolongation);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule
   that fails, soft_violations for each soft rule. False (both untouched) for an
   invalid prolongation. */
bool harmony_prolongation_check(const HarmonyTonicProlongation *prolongation, unsigned *hard_violations,
                                unsigned *soft_violations);

#endif
