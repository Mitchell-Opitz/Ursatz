#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SPECIES_BREAK_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SPECIES_BREAK_RULES_H

#include <stdbool.h>

#include "fourth_species_exercise.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Species-break rules for standard-counterpoint-v1. When neither a dissonant
 * suspension nor a consonant tied figure is possible in fourth species, it is
 * permissible to break species: a bar whose strong beat is a new note, not a
 * tie from the weak beat before it. In a broken bar the counterpoint follows
 * the principles of second species (second_species_strong_rules.h,
 * second_species_weak_rules.h), and fourth-species ties resume as soon as
 * possible.
 *
 *   BROKEN_DOWNBEAT_CONSONANT  the downbeat of a broken bar is a consonance
 *                              against the cantus firmus (hard)
 *   BROKEN_WEAK_PASSING        a weak beat in a broken bar that is a
 *                              dissonance is a dissonant passing tone: the
 *                              counterpoint passes from the consonant
 *                              downbeat to the next consonant downbeat by two
 *                              steps in the same direction (hard)
 *   BREAK_LIMITS               species is broken sparingly: in at most one
 *                              stretch of consecutive broken bars, lasting at
 *                              most two bars (soft)
 *
 * A broken bar is a middle bar (not the first, not the last) whose strong
 * beat is a new note (fourth_species_exercise.h). Every check returns false
 * for an invalid exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_SPECIES_BREAK_DOWNBEAT_CONSONANT = 0,
    STANDARD_COUNTERPOINT_SPECIES_BREAK_WEAK_PASSING,
    STANDARD_COUNTERPOINT_SPECIES_BREAK_LIMITS,
    STANDARD_COUNTERPOINT_SPECIES_BREAK_COUNT
} StandardCounterpointSpeciesBreakRule;

#define STANDARD_COUNTERPOINT_SPECIES_BREAK_MAX_STRETCHES 1
#define STANDARD_COUNTERPOINT_SPECIES_BREAK_MAX_LENGTH 2

const Rule *standard_counterpoint_species_break_rule(StandardCounterpointSpeciesBreakRule which);
const Constraint *standard_counterpoint_species_break_constraint(StandardCounterpointSpeciesBreakRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_species_break_preference(StandardCounterpointSpeciesBreakRule which);

/* "downbeat-consonant" "weak-passing" "limits"; NULL for an invalid rule. */
const char *standard_counterpoint_species_break_rule_name(StandardCounterpointSpeciesBreakRule which);

/* True when the exercise satisfies the rule. False when it does not, and for an
   invalid exercise or an invalid rule. An exercise with no broken bar
   satisfies all three. */
bool standard_counterpoint_species_break_rule_holds(StandardCounterpointSpeciesBreakRule which,
                                                    const StandardCounterpointFourthSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_species_break_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                               unsigned *hard_violations, unsigned *soft_violations);

#endif
