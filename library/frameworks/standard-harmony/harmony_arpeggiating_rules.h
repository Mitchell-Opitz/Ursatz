#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_ARPEGGIATING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_ARPEGGIATING_RULES_H

#include <stdbool.h>

#include "harmony_six_four.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Arpeggiating 6/4 rules for standard-harmony-v1, on a HarmonySixFour (harmony_six_four.h): a
 * 6/4 chord made when the bass leaps to the fifth of a chord while the upper voices sustain
 * the chord, as in a bass arpeggiation at the end of a piece or a waltz-style accompaniment.
 * It may prolong any harmony, and unlike the other types it typically is not labeled in
 * analysis: each measure could simply be the chord (I), even though the notes in the bass
 * make a 6/4 over the fifth.
 *
 * Hard:
 *
 *   IS_SIX_FOUR           the middle chord is a triad in second inversion with its root a
 *                         fourth above its bass
 *   SAME_CHORD            all three chords are on the same root: the harmony does not change
 *   BASS_LEAPS_TO_FIFTH   the bass leaps (more than a step) into the 6/4, to the fifth of
 *                         the chord
 *   UPPER_VOICES_SUSTAIN  every upper voice stays on the same note through all three chords
 *
 * Every check returns false for an invalid subject.
 */

typedef enum {
    HARMONY_ARPEGGIATING_IS_SIX_FOUR = 0,
    HARMONY_ARPEGGIATING_SAME_CHORD,
    HARMONY_ARPEGGIATING_BASS_LEAPS_TO_FIFTH,
    HARMONY_ARPEGGIATING_UPPER_VOICES_SUSTAIN,
    HARMONY_ARPEGGIATING_RULE_COUNT
} HarmonyArpeggiatingRule;

const Rule *harmony_arpeggiating_rule(HarmonyArpeggiatingRule which);
const Constraint *harmony_arpeggiating_constraint(HarmonyArpeggiatingRule which);

/* "is-six-four" "same-chord" "bass-leaps-to-fifth" "upper-voices-sustain"; NULL for an
   invalid rule. */
const char *harmony_arpeggiating_rule_name(HarmonyArpeggiatingRule which);

/* True when the arpeggiating 6/4 satisfies the rule. False when it does not, and for an
   invalid subject or an invalid rule. */
bool harmony_arpeggiating_rule_holds(HarmonyArpeggiatingRule which, const HarmonySixFour *six_four);

/* The rules it breaks: hard_violations has bit (1u << rule) set for each hard rule that
   fails; soft_violations is always 0 (there are no soft rules). False (both untouched) for
   an invalid subject. */
bool harmony_arpeggiating_check(const HarmonySixFour *six_four, unsigned *hard_violations,
                                unsigned *soft_violations);

#endif
