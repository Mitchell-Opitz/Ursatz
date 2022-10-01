#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_DECEPTIVE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_DECEPTIVE_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "harmony_deceptive.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/voice_part.h"

/*
 * Deceptive motion writing rules for standard-harmony-v1: V(7) to vi (or IV6) written in
 * four voices (harmony_deceptive.h). The subject is the two chords and their notes as
 * diatonic steps for each voice part from the soprano to the bass (harmony_steps.h; do is
 * 0, re is 1, an octave is 7). V to vi carries an inherent danger of parallels, avoided by
 * resolving ti to do as usual and moving all the other upper voices down, against the bass,
 * which goes up from sol to la.
 *
 * Hard:
 *
 *   PROGRESSION      the two chords are V or V7, then vi or IV6
 *   CHORD_NOTES      each chord has only the notes of its chord, with its root and third
 *                    present and the bass on the root (the third in IV6). V7 is four voices
 *                    on sol, ti, re, and fa, once each
 *   RESOLVES         ti goes up a step to do; in V7 to vi, fa goes down a step to mi
 *   CONTRARY_MOTION  in V(7) to vi, every upper voice but the one on ti moves down. IV6 has
 *                    no such rule
 *   NO_PARALLELS     no two voices make parallel fifths or octaves (a unison counts as an
 *                    octave)
 *
 * Soft ("less commonly"):
 *
 *   PREFER_VI        la is harmonized by vi, not IV6
 *
 * Every check returns false for an invalid subject.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[2];
    int steps[2][VOICE_PART_COUNT];
} HarmonyDeceptiveMotion;

typedef enum {
    HARMONY_DECEPTIVE_PROGRESSION = 0,
    HARMONY_DECEPTIVE_CHORD_NOTES,
    HARMONY_DECEPTIVE_RESOLVES,
    HARMONY_DECEPTIVE_CONTRARY_MOTION,
    HARMONY_DECEPTIVE_NO_PARALLELS,
    HARMONY_DECEPTIVE_PREFER_VI,
    HARMONY_DECEPTIVE_RULE_COUNT
} HarmonyDeceptiveRule;

/* True for a valid mode and two valid chords. */
bool harmony_deceptive_motion_valid(const HarmonyDeceptiveMotion *motion);

const Rule *harmony_deceptive_rule(HarmonyDeceptiveRule which);
const Constraint *harmony_deceptive_constraint(HarmonyDeceptiveRule which);

/* "progression" "chord-notes" "resolves" "contrary-motion" "no-parallels" "prefer-vi";
   NULL for an invalid rule. */
const char *harmony_deceptive_rule_name(HarmonyDeceptiveRule which);

/* True when the motion satisfies the rule. False when it does not, and for an invalid
   subject or an invalid rule. */
bool harmony_deceptive_rule_holds(HarmonyDeceptiveRule which, const HarmonyDeceptiveMotion *motion);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails, soft_violations for each soft rule. False (both untouched) for an invalid
   subject. */
bool harmony_deceptive_check(const HarmonyDeceptiveMotion *motion, unsigned *hard_violations,
                             unsigned *soft_violations);

#endif
