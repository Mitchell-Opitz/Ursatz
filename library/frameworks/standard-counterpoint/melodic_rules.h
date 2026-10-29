#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_MELODIC_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_MELODIC_RULES_H

#include <stdbool.h>

#include "counterpoint_limits.h"
#include "counterpoint_line.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Melodic writing rules for standard-counterpoint-v1: the horizontal
 * intervals of one line, for any part in any species.
 *
 *   CLAUSULA_VERA         the last note is approached by step (hard)
 *   REPEATED_NOTES        no pitch sounds more than
 *                         limits.max_consecutive_repeats times in a row
 *                         (soft)
 *   PITCH_OCCURRENCES     no pitch appears more than
 *                         limits.max_pitch_occurrences times in the whole
 *                         line (soft)
 *   CONSECUTIVE_LEAPS     no more than limits.max_consecutive_leaps leaps
 *                         in a row (soft)
 *   LEAP_DENSITY          no span of limits.leap_window consecutive
 *                         intervals holds more than
 *                         limits.max_leaps_in_window leaps (soft)
 *   REPEATED_GENERIC      no more than limits.max_same_generic_run
 *                         consecutive intervals of the same generic size,
 *                         a second then a second, whether or not they are
 *                         the same specific interval (soft)
 *   UNIQUE_CLIMAX         the highest note appears exactly once (soft)
 *   CLIMAX_NOT_FINAL      the line does not end on its highest note
 *                         (soft)
 *   CLIMAX_BY_LEAP        the highest note is reached by a leap (soft)
 *   FORBIDDEN_LEAP        no melodic leap is an augmented or diminished
 *                         interval, a seventh, a sixth other than the
 *                         ascending minor sixth, or larger than an octave
 *                         (hard)
 *   FORBIDDEN_OUTLINE     no run of notes in one direction, stepwise or
 *                         not, has first and last notes forming an
 *                         augmented or diminished interval or a seventh
 *                         (hard)
 *
 * A leap is an interval of a third or larger, compound included. A
 * climax is a line's highest pitch; the first note at that pitch is the
 * climax's position. Keeping the two parts' climaxes apart is a rule
 * about both parts and sits with the harmonic rules (harmonic_rules.h).
 *
 * Every check takes the limits it needs (counterpoint_limits.h) and
 * returns false for invalid input.
 */

typedef enum {
    STANDARD_COUNTERPOINT_MELODIC_RULE_CLAUSULA_VERA = 0,
    STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_NOTES,
    STANDARD_COUNTERPOINT_MELODIC_RULE_PITCH_OCCURRENCES,
    STANDARD_COUNTERPOINT_MELODIC_RULE_CONSECUTIVE_LEAPS,
    STANDARD_COUNTERPOINT_MELODIC_RULE_LEAP_DENSITY,
    STANDARD_COUNTERPOINT_MELODIC_RULE_REPEATED_GENERIC,
    STANDARD_COUNTERPOINT_MELODIC_RULE_UNIQUE_CLIMAX,
    STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_NOT_FINAL,
    STANDARD_COUNTERPOINT_MELODIC_RULE_CLIMAX_BY_LEAP,
    STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_LEAP,
    STANDARD_COUNTERPOINT_MELODIC_RULE_FORBIDDEN_OUTLINE,
    STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT
} StandardCounterpointMelodicRule;

const Rule *standard_counterpoint_melodic_rule(StandardCounterpointMelodicRule which);
const Constraint *standard_counterpoint_melodic_constraint(StandardCounterpointMelodicRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_melodic_preference(StandardCounterpointMelodicRule which);

/* "clausula-vera" "repeated-notes" "pitch-occurrences" "consecutive-leaps"
   "leap-density" "repeated-generic" "unique-climax" "climax-not-final"
   "climax-by-leap" "forbidden-leap" "forbidden-outline"; NULL for an
   invalid rule. */
const char *standard_counterpoint_melodic_rule_name(StandardCounterpointMelodicRule which);

/* True when the line satisfies the rule. False when it does not, and for an
   invalid line, NULL limits, or an invalid rule. */
bool standard_counterpoint_melodic_rule_holds(StandardCounterpointMelodicRule which,
                                              const StandardCounterpointLine *line,
                                              const StandardCounterpointLimits *limits);

/* The rules a line breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid line or NULL limits. */
bool standard_counterpoint_melodic_check(const StandardCounterpointLine *line,
                                         const StandardCounterpointLimits *limits, unsigned *hard_violations,
                                         unsigned *soft_violations);

/*
 * The melodic rules on a move in semitones (upward positive), for a caller that holds MIDI numbers
 * and no spellings. Each is the same rule as its spelled form.
 *
 * standard_counterpoint_melodic_semitones_allowed is FORBIDDEN_LEAP: a move of up to an octave, never
 * a tritone, never a seventh, and a sixth only as the ascending minor sixth (8 semitones up). A step
 * or a repeated note is allowed.
 *
 * standard_counterpoint_is_step: a second, 1 or 2 semitones either way. A repeated note is not a
 * step.
 */
bool standard_counterpoint_melodic_semitones_allowed(int semitones);
bool standard_counterpoint_is_step(int semitones);

#endif
