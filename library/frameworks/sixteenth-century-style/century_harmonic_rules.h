#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_HARMONIC_RULES_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_HARMONIC_RULES_H

#include <stdbool.h>

#include "century_score.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Harmonic rules for sixteenth-century-style-v1. The composers of the style
 * thought in intervals between parts, not in chords, so every rule is about
 * a pair of voices (or, for the final chord, the voices over the lowest
 * one). Two voices are compared at each slot where at least one of them
 * begins a note and both sound; a pair "moves" between two such moments when
 * the voices both change pitch.
 *
 * Hard:
 *
 *   NO_PARALLEL_PERFECTS        no two voices move in the same direction from
 *                               a fifth to a fifth or from an octave (or
 *                               unison) to an octave; a diminished fifth
 *                               counts as a fifth
 *   DIMINISHED_TRIAD_RESOLVES   where three or more voices sound a diminished
 *                               triad, the next time the voices move the
 *                               root rises a half step (ti to do), the fifth
 *                               falls a half step (fa to mi), and the third
 *                               falls a step if it moves (re to do); holds at
 *                               the end of the score
 *
 * Soft:
 *
 *   NO_DIRECT_PERFECTS          no two voices arrive at a perfect fifth or
 *                               octave by moving in the same direction,
 *                               except at the end of the pair or when
 *                               another voice moves in contrary motion to
 *                               them
 *   FINAL_CHORD                 the last slot is a bare fifth or major triad
 *                               over the lowest voice (the Picardy third
 *                               included); holds when a voice rests there or
 *                               there is only one voice
 *
 * Every check returns false for an invalid score.
 */

typedef enum {
    SIXTEENTH_CENTURY_HARMONIC_NO_PARALLEL_PERFECTS = 0,
    SIXTEENTH_CENTURY_HARMONIC_DIMINISHED_TRIAD_RESOLVES,
    SIXTEENTH_CENTURY_HARMONIC_NO_DIRECT_PERFECTS,
    SIXTEENTH_CENTURY_HARMONIC_FINAL_CHORD,
    SIXTEENTH_CENTURY_HARMONIC_RULE_COUNT
} SixteenthCenturyHarmonicRule;

const Rule *sixteenth_century_harmonic_rule(SixteenthCenturyHarmonicRule which);
const Constraint *sixteenth_century_harmonic_constraint(SixteenthCenturyHarmonicRule which);

/* "no-parallel-perfects" "diminished-triad-resolves" "no-direct-perfects" "final-chord";
   NULL for an invalid rule. */
const char *sixteenth_century_harmonic_rule_name(SixteenthCenturyHarmonicRule which);

/* True when the score satisfies the rule. False when it does not, and for an
   invalid score or an invalid rule. */
bool sixteenth_century_harmonic_rule_holds(SixteenthCenturyHarmonicRule which, const SixteenthCenturyScore *score);

/* The rules the score breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid score. */
bool sixteenth_century_harmonic_check(const SixteenthCenturyScore *score, unsigned *hard_violations,
                                      unsigned *soft_violations);

#endif
