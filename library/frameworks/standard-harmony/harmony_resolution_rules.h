#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RESOLUTION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RESOLUTION_RULES_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Resolution of one voice for standard-harmony-v1: where a voice goes from a chord to the next, for the two notes whose
 * tendencies are fixed. These rules extend the V7 resolution rules (harmony_seventh_rules.h), which cover a root-position V7
 * to a root-position I in four voices, to ONE VOICE of ANY chord with a seventh and to the leading tone of V, V7, vii-diminished
 * and its seventh chord, in any inversion, to a tonic in any number of voices. They keep the same tendencies:
 *
 *   SEVENTH_RESOLVES_DOWN   the voice that sounds the chord's seventh goes down by a diatonic step in the next chord (fa to
 *                           mi in V7). Holding it, repeating it, leaping, or going up breaks it; so does passing it to
 *                           another voice, which this rule sees as the voice leaving the seventh. The rules of
 *                           harmony_seventh_rules.h allow no suspension, hold or transfer either
 *   LEADING_TONE_RESOLVES   the voice on the leading tone (degree 7) of V, V7, vii-diminished or its seventh chord, when the
 *                           next chord is a tonic (degree 1): in an OUTER voice (the top or the bottom one) it goes up a
 *                           step to the tonic; in an INNER voice it goes up a step or, in a complete V7 only, down a third
 *                           to sol (the leading-tone drop of harmony_seventh_rules.h: sol is the closest note it can go to
 *                           without making parallels). In minor the vii-diminished and the V are on the raised seventh
 *
 * Both are hard. A rule that does not apply to the subject (the voice is not the seventh, or not the leading tone, or the
 * next chord is not a tonic) HOLDS: applicability is asked separately, so a caller reports a violation only when the rule
 * applies and fails.
 *
 * The subject is one voice's move: the two chords, the voice's diatonic steps in them (harmony_steps.h: do is 0, re is 1, an
 * octave is 7), whether the voice is outer, and, for the leading-tone drop, whether `from` is a COMPLETE V7 (root, third,
 * fifth and seventh each sounding in some voice). A step is an integer, so the direction and size of the move are plain.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord from;
    HarmonyChord to;
    int from_step;
    int to_step;
    bool outer;
    bool complete_dominant_seventh;
} HarmonyVoiceResolution;

typedef enum {
    HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN = 0,
    HARMONY_RESOLUTION_LEADING_TONE_RESOLVES,
    HARMONY_RESOLUTION_RULE_COUNT
} HarmonyResolutionRule;

/* True for a valid mode and two valid chords. */
bool harmony_resolution_valid(const HarmonyVoiceResolution *resolution);

const Rule *harmony_resolution_rule(HarmonyResolutionRule which);
const Constraint *harmony_resolution_constraint(HarmonyResolutionRule which);

/* "seventh-resolves-down" "leading-tone-resolves"; NULL for an invalid rule. */
const char *harmony_resolution_rule_name(HarmonyResolutionRule which);

/* True when the rule applies to the voice's move: for the seventh rule, the voice sounds the seventh of a seventh chord
   `from`; for the leading-tone rule, it sounds the leading tone of V, V7 or vii-diminished (or its seventh) and `to` is a tonic.
   False when it does not, and for an invalid subject or rule. */
bool harmony_resolution_rule_applies(HarmonyResolutionRule which, const HarmonyVoiceResolution *resolution);

/* True when the move satisfies the rule, which includes a rule that does not apply. False when it does not, and for an invalid
   subject or rule. */
bool harmony_resolution_rule_holds(HarmonyResolutionRule which, const HarmonyVoiceResolution *resolution);

#endif
