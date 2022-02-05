#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RHYTHM_RULES_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RHYTHM_RULES_H

#include <stdbool.h>

#include "century_score.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Rhythm rules for sixteenth-century-style-v1, applied to every voice. The
 * original notation has no bar lines, so the rules are about the tactus (the
 * half note of the modern editions): a note shorter than a tactus is
 * sub-tactus movement. All are soft.
 *
 *   SHORT_NOTES_OFF_BEAT   a note shorter than a tactus begins between tactus
 *                          starts (on an unaccented beat or off the beat as
 *                          part of a dotted rhythm), never on a tactus
 *   SLOW_START_END         a voice begins and ends with a note of at least
 *                          one tactus
 *
 * Every check returns false for an invalid score.
 */

typedef enum {
    SIXTEENTH_CENTURY_RHYTHM_SHORT_NOTES_OFF_BEAT = 0,
    SIXTEENTH_CENTURY_RHYTHM_SLOW_START_END,
    SIXTEENTH_CENTURY_RHYTHM_RULE_COUNT
} SixteenthCenturyRhythmRule;

const Rule *sixteenth_century_rhythm_rule(SixteenthCenturyRhythmRule which);
const Constraint *sixteenth_century_rhythm_constraint(SixteenthCenturyRhythmRule which);

/* "short-notes-off-beat" "slow-start-end"; NULL for an invalid rule. */
const char *sixteenth_century_rhythm_rule_name(SixteenthCenturyRhythmRule which);

/* True when the score satisfies the rule in every voice. False when it does
   not, and for an invalid score or an invalid rule. */
bool sixteenth_century_rhythm_rule_holds(SixteenthCenturyRhythmRule which, const SixteenthCenturyScore *score);

/* The rules the score breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails (none: every rule is soft), soft_violations for
   each soft rule. False (both untouched) for an invalid score. */
bool sixteenth_century_rhythm_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                    unsigned *soft_violations);

#endif
