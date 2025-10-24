#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_TENDENCY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_TENDENCY_RULES_H

#include <stdbool.h>

#include "counterpoint_line.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Melodic tendency preferences for standard-counterpoint-v1: five general
 * tendencies of melodies in Western music (Huron, Sweet Anticipation,
 * 2006), stated as soft preferences a well-formed line usually shows.
 * Failing one never makes a line invalid.
 *
 *   PITCH_PROXIMITY    steps outnumber (or equal) leaps, and small leaps
 *                      (a third or fourth) outnumber (or equal) large
 *                      ones (a fifth or more)
 *   STEP_DECLINATION   descending steps outnumber (or equal) ascending
 *                      steps
 *   STEP_INERTIA       a line continues in the same direction at least as
 *                      often as it reverses, counting each pair of
 *                      consecutive moving intervals
 *   MELODIC_REGRESSION a note in the top quarter of the line's range is
 *                      not followed by a move up, and one in the bottom
 *                      quarter is not followed by a move down: extreme
 *                      notes turn back toward the middle
 *   MELODIC_ARCH       the line rises to a climax in the middle half of
 *                      its length (not first, not last) and ends below
 *                      it
 *
 * A step is a second, a leap an interval of a third or more, compound
 * included; a repeated pitch is neither. A line with nothing to count (no
 * intervals, no pairs of moves, no range) satisfies the tendency that
 * counts it. Every check returns false for an invalid line.
 */

typedef enum {
    STANDARD_COUNTERPOINT_TENDENCY_PITCH_PROXIMITY = 0,
    STANDARD_COUNTERPOINT_TENDENCY_STEP_DECLINATION,
    STANDARD_COUNTERPOINT_TENDENCY_STEP_INERTIA,
    STANDARD_COUNTERPOINT_TENDENCY_MELODIC_REGRESSION,
    STANDARD_COUNTERPOINT_TENDENCY_MELODIC_ARCH,
    STANDARD_COUNTERPOINT_TENDENCY_COUNT
} StandardCounterpointTendency;

const Rule *standard_counterpoint_tendency_rule(StandardCounterpointTendency which);
const Constraint *standard_counterpoint_tendency_constraint(StandardCounterpointTendency which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_tendency_preference(StandardCounterpointTendency which);

/* "pitch-proximity" "step-declination" "step-inertia" "melodic-regression"
   "melodic-arch"; NULL for an invalid tendency. */
const char *standard_counterpoint_tendency_name(StandardCounterpointTendency which);

/* True when the line shows the tendency. False when it does not, and for an
   invalid line or an invalid tendency. */
bool standard_counterpoint_tendency_holds(StandardCounterpointTendency which,
                                          const StandardCounterpointLine *line);

/* The tendencies a line does not show, as a bit mask: bit (1u << tendency)
   is set for each that fails. False (*violations untouched) for an
   invalid line. */
bool standard_counterpoint_tendency_check(const StandardCounterpointLine *line, unsigned *violations);

#endif
