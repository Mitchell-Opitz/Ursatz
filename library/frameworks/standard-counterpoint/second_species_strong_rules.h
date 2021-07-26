#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "second_species_exercise.h"

/*
 * Second-species strong-beat rules for standard-counterpoint-v1: what the
 * counterpoint does on the downbeats, the strong beats at the start of
 * each bar. The downbeats against the cantus firmus follow the principles
 * of first species (first_species_harmony_rules.h), with the differences
 * the intervening weak beat makes: direct and hidden fifths and octaves
 * between successive downbeats are fine, since the weak beat diminishes
 * their effect, so no rule here forbids them.
 *
 *   CONSONANT               every downbeat is a consonance against the
 *                           cantus firmus (hard)
 *   IMPERFECT_PREFERRED     every downbeat except those of the first and
 *                           last bar is an imperfect consonance (soft)
 *   NO_STRONG_UNISON        no downbeat except those of the first and last
 *                           bar is a unison (soft); unisons are allowed on
 *                           weak beats
 *   NO_REPEATED_PERFECT     two consecutive bars do not begin with the same
 *                           perfect interval (P5 then P5, P8 then P8), and a
 *                           perfect weak beat is not followed by the same
 *                           perfect interval on the next downbeat; compound
 *                           intervals count as simple (hard)
 *   NO_DISSONANT_OUTLINE    the melodic interval between consecutive
 *                           downbeats is consonant (not augmented,
 *                           diminished, or a seventh). The one exception: a
 *                           leap of an octave from the downbeat to the weak
 *                           beat followed by a step in the opposite
 *                           direction makes a seventh with the downbeat,
 *                           and is allowed (hard)
 *   MAX_SAME_IMPERFECT_RUN  no more than three consecutive bars begin with
 *                           the same imperfect consonance (hard)
 *
 * A downbeat that is a half rest has no interval and is passed over; a rule
 * that compares two downbeats compares only bars that both sound. Every
 * check returns false for an invalid exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_CONSONANT = 0,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_IMPERFECT_PREFERRED,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_STRONG_UNISON,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_REPEATED_PERFECT,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_NO_DISSONANT_OUTLINE,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_MAX_SAME_IMPERFECT_RUN,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_STRONG_COUNT
} StandardCounterpointSecondSpeciesStrongRule;

#define STANDARD_COUNTERPOINT_SECOND_SPECIES_MAX_IMPERFECT_RUN 3

const Rule *standard_counterpoint_second_species_strong_rule(StandardCounterpointSecondSpeciesStrongRule which);
const Constraint *
standard_counterpoint_second_species_strong_constraint(StandardCounterpointSecondSpeciesStrongRule which);

/* "consonant" "imperfect-preferred" "no-strong-unison" "no-repeated-perfect"
   "no-dissonant-outline" "max-same-imperfect-run"; NULL for an invalid
   rule. */
const char *standard_counterpoint_second_species_strong_rule_name(StandardCounterpointSecondSpeciesStrongRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   an invalid exercise or an invalid rule. */
bool standard_counterpoint_second_species_strong_rule_holds(StandardCounterpointSecondSpeciesStrongRule which,
                                                            const StandardCounterpointSecondSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set
   for each hard rule that fails, soft_violations for each soft rule. False
   (both untouched) for an invalid exercise. */
bool standard_counterpoint_second_species_strong_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                       unsigned *hard_violations, unsigned *soft_violations);

#endif
