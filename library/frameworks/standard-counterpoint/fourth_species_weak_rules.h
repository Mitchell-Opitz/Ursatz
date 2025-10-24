#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_RULES_H

#include <stdbool.h>

#include "fourth_species_exercise.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Fourth-species weak-beat rules for standard-counterpoint-v1. The pattern of
 * the species invites listeners to hear the weak beats as the main
 * consonances, so each weak-beat note is a consonance and the weak-beat
 * intervals, in order, are treated as the intervals of first species
 * (first_species_harmony_rules.h). That is what governs runs of suspensions:
 * a 7-6, 4-3, 2-3, or 5-6 resolves to an imperfect consonance, and a 9-8,
 * 6-5, or 4-5 to a perfect one.
 *
 *   CONSONANT             every weak beat is a consonance against the cantus
 *                         firmus (hard), except in a bar in which species is
 *                         broken, which follows second species
 *                         (species_break_rules.h)
 *   NO_REPEATED_PERFECT   two consecutive weak beats are not the same perfect
 *                         interval, a fifth or an octave (a unison counts as an
 *                         octave); compound intervals count as simple. This
 *                         forbids two 9-8, 6-5, or 4-5 suspensions in a row and
 *                         after-beat fifths and octaves (hard)
 *   MAX_SAME_IMPERFECT_RUN  no more than three consecutive weak beats are the
 *                         same imperfect consonance, three 7-6 suspensions in
 *                         a row at most (hard)
 *   NO_DIRECT_PERFECT     no weak beat is a perfect fifth, octave, or unison
 *                         reached from the weak beat before it by similar or
 *                         parallel motion with the cantus firmus (hard)
 *   NO_INTERIOR_UNISON    no weak beat is a unison except the first and the
 *                         last (soft)
 *
 * The weak beats are those of the bars from the first to the penultimate: the
 * tie of the last bar's whole note is not a weak-beat note. Every check
 * returns false for an invalid exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_CONSONANT = 0,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_REPEATED_PERFECT,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_MAX_SAME_IMPERFECT_RUN,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_DIRECT_PERFECT,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_NO_INTERIOR_UNISON,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_WEAK_COUNT
} StandardCounterpointFourthSpeciesWeakRule;

#define STANDARD_COUNTERPOINT_FOURTH_SPECIES_MAX_IMPERFECT_RUN 3

const Rule *standard_counterpoint_fourth_species_weak_rule(StandardCounterpointFourthSpeciesWeakRule which);
const Constraint *standard_counterpoint_fourth_species_weak_constraint(StandardCounterpointFourthSpeciesWeakRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_fourth_species_weak_preference(StandardCounterpointFourthSpeciesWeakRule which);

/* "consonant" "no-repeated-perfect" "max-same-imperfect-run"
   "no-direct-perfect" "no-interior-unison"; NULL for an invalid rule. */
const char *standard_counterpoint_fourth_species_weak_rule_name(StandardCounterpointFourthSpeciesWeakRule which);

/* True when the exercise satisfies the rule. False when it does not, and for an
   invalid exercise or an invalid rule. */
bool standard_counterpoint_fourth_species_weak_rule_holds(StandardCounterpointFourthSpeciesWeakRule which,
                                                          const StandardCounterpointFourthSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_fourth_species_weak_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     unsigned *hard_violations, unsigned *soft_violations);

#endif
