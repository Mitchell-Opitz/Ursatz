#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_SEQUENCE_RULES_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_SEQUENCE_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "scale_harmonization.h"
#include "scale_patterns.h"

/*
 * Sequence rules for scale-harmonization-v1: whether a harmonization of a bass
 * scale is a given sequential pattern (scale_patterns.h). The pattern is about
 * the bass and one upper voice, the lowest upper voice (pitches[1]) of the
 * harmonization; any further voices are not looked at. Stage i of the
 * harmonization is stage (i mod unit_length) of the pattern's unit.
 *
 * Hard:
 *
 *   DIRECTION          the harmonization runs in a direction the pattern allows
 *   BASS_BY_STEP       the bass moves by a step (a second, a whole or a half
 *                      step) in the harmonization's direction at every stage
 *                      after the first where the pattern's bass moves, and
 *                      stays on its note where it does not
 *   INTERVALS          at every stage the upper voice makes an interval above the
 *                      bass that the pattern allows (generic sizes, octaves
 *                      removed); a stage with the upper voice below the bass
 *                      fails
 *   REPEATS            the pattern repeats sequentially: each stage makes the
 *                      same interval above the bass as the stage one unit before
 *                      it (so the parallel chords make the same interval
 *                      throughout)
 *   SUSPENDED          at every stage after the first where the pattern's upper
 *                      voice is suspended, it holds the pitch it had at the
 *                      stage before
 *
 * Soft:
 *
 *   WHOLE_UNITS        the harmonization is a whole number of units long
 *
 * Every check returns false for an invalid sequence.
 */

typedef struct {
    const ScalePattern *pattern;
    const ScaleHarmonization *harmonization;
} ScaleSequence;

typedef enum {
    SCALE_SEQUENCE_DIRECTION = 0,
    SCALE_SEQUENCE_BASS_BY_STEP,
    SCALE_SEQUENCE_INTERVALS,
    SCALE_SEQUENCE_REPEATS,
    SCALE_SEQUENCE_SUSPENDED,
    SCALE_SEQUENCE_WHOLE_UNITS,
    SCALE_SEQUENCE_RULE_COUNT
} ScaleSequenceRule;

const Rule *scale_sequence_rule(ScaleSequenceRule which);
const Constraint *scale_sequence_constraint(ScaleSequenceRule which);

/* "direction" "bass-by-step" "intervals" "repeats" "suspended" "whole-units"; NULL for an
   invalid rule. */
const char *scale_sequence_rule_name(ScaleSequenceRule which);

/* True when the sequence has a pattern and a valid harmonization. */
bool scale_sequence_valid(const ScaleSequence *sequence);

/* True when the harmonization satisfies the rule for the pattern. False when it
   does not, and for an invalid sequence or an invalid rule. */
bool scale_sequence_rule_holds(ScaleSequenceRule which, const ScaleSequence *sequence);

/* The rules the harmonization breaks: hard_violations has bit (1u << rule) set
   for each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid sequence. */
bool scale_sequence_check(const ScaleSequence *sequence, unsigned *hard_violations, unsigned *soft_violations);

#endif
