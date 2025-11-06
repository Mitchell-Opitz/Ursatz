#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_RULES_H

#include <stdbool.h>

#include "fourth_species_exercise.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Fourth-species counterpoint line rules for standard-counterpoint-v1: the
 * counterpoint line. Using dissonant suspensions whenever possible gives a
 * line of mostly downward stepwise motion that is hard to direct toward a
 * climax, so the shape rules of the other species (a single climax,
 * secondary climaxes) are not asked for: the line must be smooth and
 * singable, and the suspensions properly prepared and resolved. The rules read
 * the line as its attacks (fourth_species_exercise.h): the new notes in order,
 * with the rest and the ties left out.
 *
 *   LEAPS           every melodic leap is a consonant melodic interval: no
 *                   augmented or diminished interval, no seventh, nothing
 *                   larger than an octave, and the only sixth is the ascending
 *                   minor sixth (hard)
 *   FINAL_STEP      the last note is approached by step, from above, or from
 *                   below by a half step (hard); the cantus firmus rule
 *   RANGE           the lowest and highest notes are no more than a tenth
 *                   apart (hard); the cantus firmus rule
 *   LEADING_TONE    a leading tone steps up to the tonic, and a seventh degree
 *                   a whole step below the tonic does not (hard); the cantus
 *                   firmus rule
 *   DOWNWARD_STEPS  at least half of the progressions are steps down (soft)
 *   UPWARD_LEAP     the line leaps up at least once, to counteract the
 *                   downward resolutions and keep the line in a singable range
 *                   (soft)
 *
 * A leap is an interval of a third or more. Every check needs the tonic and
 * returns false for an invalid exercise or a NULL tonic.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_LEAPS = 0,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_FINAL_STEP,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_RANGE,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_LEADING_TONE,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_DOWNWARD_STEPS,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_UPWARD_LEAP,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_LINE_COUNT
} StandardCounterpointFourthSpeciesLineRule;

const Rule *standard_counterpoint_fourth_species_line_rule(StandardCounterpointFourthSpeciesLineRule which);
const Constraint *standard_counterpoint_fourth_species_line_constraint(StandardCounterpointFourthSpeciesLineRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_fourth_species_line_preference(StandardCounterpointFourthSpeciesLineRule which);

/* "leaps" "final-step" "range" "leading-tone" "downward-steps" "upward-leap";
   NULL for an invalid rule. */
const char *standard_counterpoint_fourth_species_line_rule_name(StandardCounterpointFourthSpeciesLineRule which);

/* True when the counterpoint line satisfies the rule. False when it does not,
   and for invalid input. */
bool standard_counterpoint_fourth_species_line_rule_holds(StandardCounterpointFourthSpeciesLineRule which,
                                                          const StandardCounterpointFourthSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic);

/* The rules the line breaks: hard_violations has bit (1u << rule) set for each
   hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for invalid input. */
bool standard_counterpoint_fourth_species_line_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *hard_violations,
                                                     unsigned *soft_violations);

#endif
