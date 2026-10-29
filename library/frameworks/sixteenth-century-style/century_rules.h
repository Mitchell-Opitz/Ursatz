#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RULES_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "century_imitation_rules.h"
#include "century_score.h"
#include "standard-counterpoint/slot_note.h"

/*
 * Checking a whole score against the rule groups of
 * sixteenth-century-style-v1:
 *
 *   MELODIC      century_melodic_rules.h, on every voice
 *   HARMONIC     century_harmonic_rules.h, on the voices together
 *   DISSONANCE   century_dissonance_rules.h, on the voices together
 *   RHYTHM       century_rhythm_rules.h, on every voice (soft only)
 *   IMITATION    century_imitation_rules.h, on every point of imitation the
 *                caller names (skipped when there are none)
 *
 * The result sets a bit (1u << group) in hard_violations for each group with
 * a hard rule broken, and in soft_violations for each group with a soft rule
 * broken. A score is valid when no hard rule is broken; soft violations are
 * advice. To learn exactly which rule failed, ask the group's own check.
 */

typedef enum {
    SIXTEENTH_CENTURY_GROUP_MELODIC = 0,
    SIXTEENTH_CENTURY_GROUP_HARMONIC,
    SIXTEENTH_CENTURY_GROUP_DISSONANCE,
    SIXTEENTH_CENTURY_GROUP_RHYTHM,
    SIXTEENTH_CENTURY_GROUP_IMITATION,
    SIXTEENTH_CENTURY_GROUP_COUNT
} SixteenthCenturyGroup;

/* "melodic" "harmonic" "dissonance" "rhythm" "imitation"; NULL for an invalid
   group. */
const char *sixteenth_century_group_name(SixteenthCenturyGroup group);

/* The score and the points of imitation in it (the points' scores must be
   this score; `points` may be NULL when `point_count` is 0). */
typedef struct {
    const SixteenthCenturyScore *score;
    const SixteenthCenturyPoint *points;
    size_t point_count;
} SixteenthCenturyInput;

/* The hard and soft violations of a run (standard-counterpoint/slot_note.h). */
typedef StandardCounterpointRuleResult SixteenthCenturyResult;

/* Runs the groups over the score. False (*out untouched) for invalid input:
   NULL, an invalid score, a point whose score is not this one, or an invalid
   point. */
bool sixteenth_century_check(const SixteenthCenturyInput *input, SixteenthCenturyResult *out);

/* True when the input is valid and breaks no hard rule. */
bool sixteenth_century_valid(const SixteenthCenturyInput *input);

#endif
