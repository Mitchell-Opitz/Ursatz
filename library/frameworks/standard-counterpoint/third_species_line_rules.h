#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "third_species_exercise.h"

/*
 * Third-species counterpoint line rules for standard-counterpoint-v1: the
 * shape of the counterpoint line. As in the other species the line is a good
 * melody, and with four notes to a bar it is even more stepwise, because few
 * situations call for a leap. A leap, when needed, is better inside a bar
 * than across the bar line. The rules read the line as its attacks
 * (third_species_exercise.h): the new notes in order, with rests and ties
 * left out. Which dissonances are allowed, and where, is third_species_beat_rules.h.
 *
 *   LEAPS               no melodic leap is an augmented or diminished
 *                       interval, a seventh, or larger than an octave, and
 *                       the only sixth is the ascending minor sixth: every
 *                       leap is a consonant melodic interval (hard)
 *   FINAL_STEP          the last note is approached by step, from above, or
 *                       from below by a half step (hard); the cantus
 *                       firmus rule of the same name
 *   RANGE               the lowest and highest notes are no more than a
 *                       tenth apart (hard); the cantus firmus rule
 *   LEADING_TONE        a leading tone steps up to the tonic, and a seventh
 *                       degree a whole step below the tonic does not (hard);
 *                       the cantus firmus rule
 *   SINGLE_CLIMAX       the highest note appears only once (soft); the
 *                       cantus firmus rule. Keeping it apart from the cantus
 *                       firmus climax is a spacing rule
 *   NO_REPEATED_MOTIVE  no run of four notes sounds twice (soft); the cantus
 *                       firmus rule
 *   STEPWISE            at least three quarters of the progressions are steps
 *                       (soft)
 *   LEAP_PLACEMENT      leaps inside a bar are at least as many as leaps
 *                       across the bar line (soft)
 *   SECONDARY_CLIMAXES  with at least eight notes, the line has one or two
 *                       secondary climaxes: notes lower than the overall
 *                       climax that are higher than the notes on either side
 *                       (soft)
 *
 * The cantus firmus rules that limit leaps (leap recovery and consecutive
 * leaps) are not applied: the dissonance figures include leaps of a third
 * (the double neighbor and the nota cambiata), and the recovery of a large
 * leap is a rule of its own (third_species_beat_rules.h). The cantus firmus
 * rules for length, the tonic, melodic consonance, and stepwise motion are
 * replaced by the rules above. A leap is an interval of a third or more.
 *
 * Every check needs the tonic and returns false for an invalid exercise or a
 * NULL tonic.
 */

typedef enum {
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEAPS = 0,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_FINAL_STEP,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_RANGE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEADING_TONE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_SINGLE_CLIMAX,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_NO_REPEATED_MOTIVE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_STEPWISE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_LEAP_PLACEMENT,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_SECONDARY_CLIMAXES,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_LINE_COUNT
} StandardCounterpointThirdSpeciesLineRule;

#define STANDARD_COUNTERPOINT_THIRD_SPECIES_MIN_ATTACKS_FOR_SECONDARY_CLIMAXES 8
#define STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_SECONDARY_CLIMAXES 2

const Rule *standard_counterpoint_third_species_line_rule(StandardCounterpointThirdSpeciesLineRule which);
const Constraint *standard_counterpoint_third_species_line_constraint(StandardCounterpointThirdSpeciesLineRule which);

/* "leaps" "final-step" "range" "leading-tone" "single-climax"
   "no-repeated-motive" "stepwise" "leap-placement" "secondary-climaxes";
   NULL for an invalid rule. */
const char *standard_counterpoint_third_species_line_rule_name(StandardCounterpointThirdSpeciesLineRule which);

/* True when the counterpoint line satisfies the rule. False when it does
   not, and for invalid input. */
bool standard_counterpoint_third_species_line_rule_holds(StandardCounterpointThirdSpeciesLineRule which,
                                                          const StandardCounterpointThirdSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic);

/* The rules the line breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for invalid input. */
bool standard_counterpoint_third_species_line_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations);

#endif
