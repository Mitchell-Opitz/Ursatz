#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_EMBELLISHING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_EMBELLISHING_RULES_H

#include <stdbool.h>

#include "harmony_embellishing.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Embellishing tone rules for standard-harmony-v1, on a gesture
 * (harmony_embellishing.h).
 *
 * Hard:
 *
 *   CLASSIFIABLE            the gesture is one of the eight embellishing tones: a
 *                           passing tone, neighbor tone, appoggiatura, escape
 *                           tone, suspension, retardation, pedal, or
 *                           anticipation
 *   STRONGER_BEAT           a suspension or retardation is always on a stronger
 *                           part of the beat than the notes around it; this
 *                           always holds for the others
 *
 * Soft ("in nearly all cases", "almost always", "typically", "usually", "more
 * common"):
 *
 *   CHORD_TONE_ENDS         the first and last notes of a three-note gesture are
 *                           chord tones, consonant with the bass; in an
 *                           anticipation the second note is
 *   MIDDLE_NOT_CHORD_TONE   the embellishing tone is not a chord tone: the middle
 *                           note, or the first note of an anticipation
 *   APPOGGIATURA_STRONG_BEAT
 *                           an appoggiatura is on a stronger part of the beat
 *                           than the notes around it
 *   ESCAPE_TONE_WEAK_BEAT   an escape tone is on a weaker part of the beat than
 *                           the notes around it
 *   ANTICIPATION_WEAK_BEAT  an anticipation is on a weaker part of the beat than
 *                           the note after it
 *   LEFT_DOWNWARD           an appoggiatura or escape tone is left downward, not
 *                           upward
 *
 * A rule about one tone always holds for any other gesture. Every check returns
 * false for an invalid gesture.
 */

typedef enum {
    HARMONY_EMBELLISHING_CLASSIFIABLE = 0,
    HARMONY_EMBELLISHING_STRONGER_BEAT,
    HARMONY_EMBELLISHING_CHORD_TONE_ENDS,
    HARMONY_EMBELLISHING_MIDDLE_NOT_CHORD_TONE,
    HARMONY_EMBELLISHING_APPOGGIATURA_STRONG_BEAT,
    HARMONY_EMBELLISHING_ESCAPE_TONE_WEAK_BEAT,
    HARMONY_EMBELLISHING_ANTICIPATION_WEAK_BEAT,
    HARMONY_EMBELLISHING_LEFT_DOWNWARD,
    HARMONY_EMBELLISHING_RULE_COUNT
} HarmonyEmbellishingRule;

const Rule *harmony_embellishing_rule(HarmonyEmbellishingRule which);
const Constraint *harmony_embellishing_constraint(HarmonyEmbellishingRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *harmony_embellishing_preference(HarmonyEmbellishingRule which);

/* "classifiable" "stronger-beat" "chord-tone-ends" "middle-not-chord-tone"
   "appoggiatura-strong-beat" "escape-tone-weak-beat" "anticipation-weak-beat"
   "left-downward"; NULL for an invalid rule. */
const char *harmony_embellishing_rule_name(HarmonyEmbellishingRule which);

/* True when the gesture satisfies the rule. False when it does not, and for an
   invalid gesture or an invalid rule. */
bool harmony_embellishing_rule_holds(HarmonyEmbellishingRule which, const HarmonyGesture *gesture);

/* The rules the gesture breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid gesture. */
bool harmony_embellishing_check(const HarmonyGesture *gesture, unsigned *hard_violations,
                                unsigned *soft_violations);

#endif
