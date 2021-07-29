#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "second_species_exercise.h"

/*
 * Second-species weak-beat rules for standard-counterpoint-v1: what the
 * counterpoint does on the weak beats, where dissonance is first allowed.
 * The patterns a weak beat can follow are in second_species_pattern.h.
 *
 *   DISSONANT_PASSING     every weak beat that is a dissonance against the
 *                         cantus firmus is a dissonant passing tone: the
 *                         counterpoint passes from a consonant downbeat to
 *                         the next consonant downbeat by two steps in the
 *                         same direction, so it does not leap into or out
 *                         of the dissonance, change direction on it, or
 *                         put it on a downbeat (hard)
 *   PATTERN_FIT           every weak beat that is a consonance follows one
 *                         of the consonant patterns, a guide for both
 *                         composing and evaluating the counterpoint (soft)
 *   MIXTURE               a mixture of consonant and dissonant weak beats
 *                         promotes variety: with at least four weak beats,
 *                         at least one is a consonance and at least one a
 *                         dissonance (soft)
 *   REGISTER_CHANGE_RARE  the change of register is used infrequently: at
 *                         most once in an exercise (soft)
 *
 * Unisons are allowed on weak beats: when they come from smooth voice
 * leading, the difference in rhythm keeps the lines independent, so no rule
 * here forbids them. A weak beat that is a tie (a whole note) is not a new
 * note, and a bar whose downbeat is a half rest, or the last bar, has no
 * pattern; these are passed over by PATTERN_FIT (a dissonant weak beat there
 * breaks DISSONANT_PASSING, since it has no downbeat or next downbeat to
 * pass between). Every check returns false for an invalid exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_DISSONANT_PASSING = 0,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_PATTERN_FIT,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_MIXTURE,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_REGISTER_CHANGE_RARE,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_WEAK_COUNT
} StandardCounterpointSecondSpeciesWeakRule;

#define STANDARD_COUNTERPOINT_SECOND_SPECIES_MIN_WEAK_FOR_MIXTURE 4
#define STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_REGISTER_CHANGES 1

const Rule *standard_counterpoint_second_species_weak_rule(StandardCounterpointSecondSpeciesWeakRule which);
const Constraint *standard_counterpoint_second_species_weak_constraint(StandardCounterpointSecondSpeciesWeakRule which);

/* "dissonant-passing" "pattern-fit" "mixture" "register-change-rare"; NULL
   for an invalid rule. */
const char *standard_counterpoint_second_species_weak_rule_name(StandardCounterpointSecondSpeciesWeakRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   an invalid exercise or an invalid rule. */
bool standard_counterpoint_second_species_weak_rule_holds(StandardCounterpointSecondSpeciesWeakRule which,
                                                          const StandardCounterpointSecondSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set
   for each hard rule that fails, soft_violations for each soft rule. False
   (both untouched) for an invalid exercise. */
bool standard_counterpoint_second_species_weak_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     unsigned *hard_violations, unsigned *soft_violations);

#endif
