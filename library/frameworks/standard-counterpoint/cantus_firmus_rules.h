#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_CANTUS_FIRMUS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_CANTUS_FIRMUS_RULES_H

#include <stdbool.h>

#include "counterpoint_line.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Cantus firmus rules for standard-counterpoint-v1. A cantus firmus
 * ("fixed melody") is the single well-formed line every species exercise
 * is written against. These rules cover what makes one well formed:
 *
 *   LENGTH              8 to 16 notes
 *   TONIC_ENDS          begins and ends on the tonic ("do"), at any
 *                       octave, compared by letter and accidental
 *   FINAL_STEP          the last note is approached by step: usually from
 *                       the supertonic (re to do, from above, a whole or
 *                       half step), sometimes from the leading tone (ti
 *                       to do, from below, which must be a half step: the
 *                       seventh degree is raised in "minor"-type modes)
 *   MELODIC_CONSONANCE  every note-to-note progression is a consonant
 *                       melodic interval (consonance_rules.h)
 *   RANGE               the lowest and highest notes are no more than a
 *                       tenth apart
 *   SINGLE_CLIMAX       the highest note appears only once (soft)
 *   LEAP_RECOVERY       a leap of a fourth or larger is followed by a
 *                       step in the opposite direction
 *   CONSECUTIVE_LEAPS   no more than two leaps in a row and no two leaps
 *                       in a row in the same direction, except two thirds
 *                       in the same direction that together outline a
 *                       perfect fifth, a consonant triad (as in the
 *                       F-mode cantus firmus)
 *   LEADING_TONE        a leading tone (a half step below the tonic, on
 *                       the letter below it) goes up to the tonic; a
 *                       seventh degree a whole step below the tonic does
 *                       not step up to it, since it must be raised there
 *   MOSTLY_STEPWISE     at least half the progressions are steps (soft)
 *   NO_REPEATED_MOTIVE  no run of three notes sounds twice (soft)
 *
 * The first eight hard rules say what a cantus firmus must be; the last
 * three soft ones are tendencies ("usually", "mostly", "no licks"). A leap
 * is an interval of a third or larger.
 *
 * Not covered: that all notes are whole notes (a Line carries no rhythm),
 * and the smooth overall shape from beginning to climax to ending, which
 * is the melodic arch of tendency_rules.h.
 *
 * Every check takes the line and the tonic of the mode (a spelling; its
 * octave is ignored) and returns false for invalid input.
 */

typedef enum {
    STANDARD_COUNTERPOINT_CANTUS_RULE_LENGTH = 0,
    STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS,
    STANDARD_COUNTERPOINT_CANTUS_RULE_FINAL_STEP,
    STANDARD_COUNTERPOINT_CANTUS_RULE_MELODIC_CONSONANCE,
    STANDARD_COUNTERPOINT_CANTUS_RULE_RANGE,
    STANDARD_COUNTERPOINT_CANTUS_RULE_SINGLE_CLIMAX,
    STANDARD_COUNTERPOINT_CANTUS_RULE_LEAP_RECOVERY,
    STANDARD_COUNTERPOINT_CANTUS_RULE_CONSECUTIVE_LEAPS,
    STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE,
    STANDARD_COUNTERPOINT_CANTUS_RULE_MOSTLY_STEPWISE,
    STANDARD_COUNTERPOINT_CANTUS_RULE_NO_REPEATED_MOTIVE,
    STANDARD_COUNTERPOINT_CANTUS_RULE_COUNT
} StandardCounterpointCantusRule;

#define STANDARD_COUNTERPOINT_CANTUS_MIN_LENGTH 8
#define STANDARD_COUNTERPOINT_CANTUS_MAX_LENGTH 16
#define STANDARD_COUNTERPOINT_CANTUS_MAX_RANGE 10

const Rule *standard_counterpoint_cantus_rule(StandardCounterpointCantusRule which);
const Constraint *standard_counterpoint_cantus_constraint(StandardCounterpointCantusRule which);

/* "length" "tonic-ends" "final-step" "melodic-consonance" "range"
   "single-climax" "leap-recovery" "consecutive-leaps" "leading-tone"
   "mostly-stepwise" "no-repeated-motive"; NULL for an invalid rule. */
const char *standard_counterpoint_cantus_rule_name(StandardCounterpointCantusRule which);

/* True when the line satisfies the rule. False when it does not, and for
   an invalid line, a NULL tonic, or an invalid rule. */
bool standard_counterpoint_cantus_rule_holds(StandardCounterpointCantusRule which,
                                             const StandardCounterpointLine *line, const PitchSpelling *tonic);

/* The rules a cantus firmus breaks: hard_violations has bit
   (1u << rule) set for each hard rule that fails, soft_violations for each
   soft rule. False (both untouched) for an invalid line or a NULL tonic. */
bool standard_counterpoint_cantus_check(const StandardCounterpointLine *line, const PitchSpelling *tonic,
                                        unsigned *hard_violations, unsigned *soft_violations);

#endif
