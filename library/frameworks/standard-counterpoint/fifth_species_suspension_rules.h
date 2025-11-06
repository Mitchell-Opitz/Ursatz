#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULES_H

#include <stdbool.h>

#include "fifth_species_exercise.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Fifth-species suspension rules for standard-counterpoint-v1: how the
 * counterpoint handles the suspension (fifth_species_figure.h), the one
 * dissonance that sounds on a downbeat. A suspension is a tied note: the
 * preparation, the note tied across the bar line and dissonant against the
 * next cantus firmus note, and the resolution, the next note, a step below it.
 * Fifth species allows the held note to be a quarter, a half, or a dotted
 * half note, so the same figure appears with the embellishments of
 * fifth species between the resolution and the next bar.
 *
 *   PROPER          every tied downbeat that is a dissonance against the
 *                   cantus firmus is a proper suspension: a consonant
 *                   preparation, a tie into the dissonance, and a consonant
 *                   resolution a step below, as 7-6, 4-3, or 9-8 above the
 *                   cantus firmus or 2-3 below it (hard)
 *   USED            an exercise of at least five bars holds at least one
 *                   dissonant suspension (soft)
 *   FEW_NINE_EIGHT  7-6 and 4-3 are preferable to 9-8, which resolves to a
 *                   perfect consonance: no more than half of the dissonant
 *                   suspensions are 9-8 (soft)
 *
 * Every check returns false for an invalid exercise.
 */

#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_MIN_BARS 5

typedef enum {
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_PROPER = 0,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_USED,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_FEW_NINE_EIGHT,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_SUSPENSION_RULE_COUNT
} StandardCounterpointFifthSpeciesSuspensionRule;

const Rule *standard_counterpoint_fifth_species_suspension_rule(StandardCounterpointFifthSpeciesSuspensionRule which);
const Constraint *
standard_counterpoint_fifth_species_suspension_constraint(StandardCounterpointFifthSpeciesSuspensionRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_fifth_species_suspension_preference(StandardCounterpointFifthSpeciesSuspensionRule which);

/* "proper" "used" "few-nine-eight"; NULL for an invalid rule. */
const char *
standard_counterpoint_fifth_species_suspension_rule_name(StandardCounterpointFifthSpeciesSuspensionRule which);

/* True when the exercise satisfies the rule. False when it does not, and for an
   invalid exercise or an invalid rule. */
bool standard_counterpoint_fifth_species_suspension_rule_holds(
    StandardCounterpointFifthSpeciesSuspensionRule which, const StandardCounterpointFifthSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_fifth_species_suspension_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations);

#endif
