#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "third_species_exercise.h"

/*
 * Third-species strong-beat rules for standard-counterpoint-v1: what the
 * counterpoint does on the downbeats, and how it moves from the last beat of a
 * bar into the next downbeat. They follow second species
 * (second_species_strong_rules.h) with these differences: two bars in a row
 * may begin with the same perfect interval but three may not, and the
 * downbeats get no octave-leap exception. Direct fifths and octaves between
 * successive downbeats are allowed, since the notes between them diminish their
 * effect, so no rule here forbids them.
 *
 *   CONSONANT                  every downbeat is a consonance against the
 *                              cantus firmus (hard)
 *   IMPERFECT_PREFERRED        every downbeat except those of the first and
 *                              last bar is an imperfect consonance (soft)
 *   NO_STRONG_UNISON           no downbeat except those of the first and last
 *                              bar is a unison (soft); unisons are allowed on
 *                              other beats
 *   NO_THREE_PERFECT           no three consecutive bars begin with the same
 *                              perfect interval; two in a row are fine.
 *                              Compound intervals count as simple (hard)
 *   MAX_SAME_IMPERFECT_RUN     no more than three consecutive bars begin with
 *                              the same imperfect consonance (hard)
 *   NO_DISSONANT_OUTLINE       the melodic interval between the pitches that
 *                              begin consecutive bars is consonant: not
 *                              augmented, diminished, or a seventh (hard)
 *   PREVIOUS_BAR_PERFECTS      when a downbeat is a perfect fifth, neither beat
 *                              3 nor beat 4 of the bar before it is a perfect
 *                              fifth; when a downbeat is an octave (a unison
 *                              counts), none of beats 2, 3, and 4 of the bar
 *                              before it is an octave (hard). Parallel fifths
 *                              and octaves are not made acceptable by notes
 *                              between them
 *   BAR_LINE_REPEATED_PERFECT  beat 4 and the downbeat after it are not the
 *                              same perfect interval (hard)
 *   BAR_LINE_DIRECT_PERFECT    the downbeat after beat 4 is not a perfect
 *                              fifth, octave, or unison reached by similar or
 *                              parallel motion with the cantus firmus (hard):
 *                              the motion from beat 4 into a downbeat follows
 *                              first species
 *
 * A downbeat that is a quarter rest has no interval and is passed over; a
 * rule that compares two bars compares only bars whose beats sound. Every
 * check returns false for an invalid exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_CONSONANT = 0,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_IMPERFECT_PREFERRED,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_STRONG_UNISON,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_THREE_PERFECT,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_MAX_SAME_IMPERFECT_RUN,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_NO_DISSONANT_OUTLINE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_PREVIOUS_BAR_PERFECTS,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_BAR_LINE_REPEATED_PERFECT,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_BAR_LINE_DIRECT_PERFECT,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_STRONG_COUNT
} StandardCounterpointThirdSpeciesStrongRule;

#define STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_PERFECT_RUN 2
#define STANDARD_COUNTERPOINT_THIRD_SPECIES_MAX_IMPERFECT_RUN 3

const Rule *standard_counterpoint_third_species_strong_rule(StandardCounterpointThirdSpeciesStrongRule which);
const Constraint *
standard_counterpoint_third_species_strong_constraint(StandardCounterpointThirdSpeciesStrongRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_third_species_strong_preference(StandardCounterpointThirdSpeciesStrongRule which);

/* "consonant" "imperfect-preferred" "no-strong-unison" "no-three-perfect"
   "max-same-imperfect-run" "no-dissonant-outline" "previous-bar-perfects"
   "bar-line-repeated-perfect" "bar-line-direct-perfect"; NULL for an invalid
   rule. */
const char *standard_counterpoint_third_species_strong_rule_name(StandardCounterpointThirdSpeciesStrongRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   an invalid exercise or an invalid rule. */
bool standard_counterpoint_third_species_strong_rule_holds(StandardCounterpointThirdSpeciesStrongRule which,
                                                           const StandardCounterpointThirdSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_third_species_strong_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                      unsigned *hard_violations, unsigned *soft_violations);

#endif
