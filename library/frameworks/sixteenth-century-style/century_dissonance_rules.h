#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_DISSONANCE_RULES_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_DISSONANCE_RULES_H

#include <stdbool.h>

#include "century_score.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Dissonance rules for sixteenth-century-style-v1. A dissonance is the
 * interval between two voices at a slot where both sound: a second, a
 * seventh, any augmented or diminished interval, and a fourth against the
 * lowest sounding voice. Perfect and imperfect consonances are the unison,
 * third, fifth, sixth, and octave, compound included. A fourth between two
 * upper voices is consonant.
 *
 * The rules look at each new dissonance, a slot where one of the two
 * voices begins a note and the pair is dissonant. A dissonance that only
 * continues from an earlier slot was judged where it began. It is one of:
 *
 *   suspension   on a tactus start (strong or weak), one voice holds a note
 *                (a tie) and the other begins a note
 *   figure       on a position between tactus starts (sub-tactus), a voice
 *                begins a note that is a passing or neighbor tone (approached
 *                and left by step) or a nota cambiata (approached by a step
 *                down, left by a leap down of a third, then a step up: do, ti,
 *                sol, la), the only dissonance left by leap
 *
 * Hard:
 *
 *   FIGURED_DISSONANCE      every new dissonance is a suspension (judged by
 *                           PROPER_SUSPENSIONS) or, between tactus starts, a
 *                           figure; no voice begins a dissonant note on a
 *                           tactus start, and no sub-tactus dissonance is
 *                           anything but a figure
 *   PROPER_SUSPENSIONS      the held note was prepared: it began on a weaker
 *                           position than the suspension (the position
 *                           ranking is strong, weak, sub-tactus) and was
 *                           consonant where it began; and it resolves: the
 *                           voice then steps down to a consonance. The
 *                           dissonance is a 7-6 or 4-3 in the upper voice
 *                           and a 2-3 in the lower voice
 *
 * Soft:
 *
 *   FIGURE_RESOLVES         a figure dissonance is followed by a consonance,
 *                           where its next note begins
 *
 * Every check returns false for an invalid score.
 */

typedef enum {
    SIXTEENTH_CENTURY_DISSONANCE_FIGURED = 0,
    SIXTEENTH_CENTURY_DISSONANCE_PROPER_SUSPENSIONS,
    SIXTEENTH_CENTURY_DISSONANCE_FIGURE_RESOLVES,
    SIXTEENTH_CENTURY_DISSONANCE_RULE_COUNT
} SixteenthCenturyDissonanceRule;

const Rule *sixteenth_century_dissonance_rule(SixteenthCenturyDissonanceRule which);
const Constraint *sixteenth_century_dissonance_constraint(SixteenthCenturyDissonanceRule which);

/* "figured" "proper-suspensions" "figure-resolves"; NULL for an invalid rule. */
const char *sixteenth_century_dissonance_rule_name(SixteenthCenturyDissonanceRule which);

/* True when the pitches of voices `a` and `b` at `slot` are a consonance
   (a fourth against the lowest sounding voice is not). False for a
   dissonance, and for an invalid score, a voice out of range, or a rest. */
bool sixteenth_century_consonant_at(const SixteenthCenturyScore *score, size_t a, size_t b, size_t slot);

/* True when the score satisfies the rule. False when it does not, and for an
   invalid score or an invalid rule. */
bool sixteenth_century_dissonance_rule_holds(SixteenthCenturyDissonanceRule which,
                                             const SixteenthCenturyScore *score);

/* The rules the score breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid score. */
bool sixteenth_century_dissonance_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                        unsigned *soft_violations);

#endif
