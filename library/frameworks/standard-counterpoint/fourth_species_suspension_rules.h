#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULES_H

#include <stdbool.h>

#include "fourth_species_exercise.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Fourth-species suspension rules for standard-counterpoint-v1: how the
 * counterpoint handles dissonance. The suspension (fourth_species_suspension.h)
 * is the one dissonance of the species: an accented dissonance on a strong
 * beat, so it is always preceded and followed by consonances.
 *
 *   PROPER_SUSPENSION   every tied downbeat that is a dissonance against the
 *                       cantus firmus is a dissonant suspension: a
 *                       consonant preparation, a tie into the dissonance, and
 *                       a consonant resolution a step below, as 7-6, 4-3, or
 *                       9-8 above the cantus firmus or 2-3 below it (hard).
 *                       A new note on a downbeat is a bar in which species is
 *                       broken, and is judged by the species-break rules
 *   MANY_SUSPENSIONS    dissonant suspensions are used as much as possible:
 *                       at least half of the tied middle bars hold one (soft)
 *   FEW_NINE_EIGHT      7-6 and 4-3 are preferable to 9-8, which resolves to a
 *                       perfect consonance: no more than half of the
 *                       dissonant suspensions are 9-8 (soft)
 *
 * A middle bar is any bar but the first and the last. Every check returns
 * false for an invalid exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_PROPER = 0,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_MANY,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_FEW_NINE_EIGHT,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_SUSPENSION_RULE_COUNT
} StandardCounterpointFourthSpeciesSuspensionRule;

const Rule *standard_counterpoint_fourth_species_suspension_rule(StandardCounterpointFourthSpeciesSuspensionRule which);
const Constraint *
standard_counterpoint_fourth_species_suspension_constraint(StandardCounterpointFourthSpeciesSuspensionRule which);

/* "proper" "many" "few-nine-eight"; NULL for an invalid rule. */
const char *
standard_counterpoint_fourth_species_suspension_rule_name(StandardCounterpointFourthSpeciesSuspensionRule which);

/* True when the exercise satisfies the rule. False when it does not, and for an
   invalid exercise or an invalid rule. */
bool standard_counterpoint_fourth_species_suspension_rule_holds(
    StandardCounterpointFourthSpeciesSuspensionRule which, const StandardCounterpointFourthSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_fourth_species_suspension_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations);

#endif
