#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "third_species_exercise.h"

/*
 * Third-species rules for the beats other than the downbeat, for
 * standard-counterpoint-v1: where dissonance may fall and how the line moves
 * in and out of consonances. The downbeat rules are in
 * third_species_strong_rules.h, and the leap rule of the line (every leap a
 * consonant melodic interval) in third_species_line_rules.h.
 *
 *   DISSONANCE_FIGURE    every dissonant new note on beat 2, 3, or 4 belongs
 *                        to a figure (third_species_figure.h): a passing
 *                        tone, a pair of passing tones, a neighbor tone, a
 *                        double neighbor, or a nota cambiata. So a
 *                        dissonance is approached and left by step, except
 *                        in the double neighbor and the nota cambiata, and
 *                        no run of three dissonances is allowed (hard). A
 *                        dissonance on a downbeat is the strong-beat rule
 *   MIXTURE              with at least four bars, beats 2 to 4 hold at least
 *                        one dissonance and at least one consonance, for
 *                        variety (soft)
 *   LARGE_LEAP_RECOVERY  a leap of a fourth or larger is followed by a step
 *                        in the opposite direction (soft); a leap into the
 *                        last note has nothing to follow it
 *
 * The counterpoint moves in and out of consonant tones freely by step and by
 * consonant leap, and unisons are allowed on every beat but the downbeat, so
 * no rule here limits them. Every check returns false for an invalid
 * exercise.
 */

typedef enum {
    STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_DISSONANCE_FIGURE = 0,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_MIXTURE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_LARGE_LEAP_RECOVERY,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_BEAT_COUNT
} StandardCounterpointThirdSpeciesBeatRule;

#define STANDARD_COUNTERPOINT_THIRD_SPECIES_MIN_BARS_FOR_MIXTURE 4
#define STANDARD_COUNTERPOINT_THIRD_SPECIES_LARGE_LEAP 4

const Rule *standard_counterpoint_third_species_beat_rule(StandardCounterpointThirdSpeciesBeatRule which);
const Constraint *standard_counterpoint_third_species_beat_constraint(StandardCounterpointThirdSpeciesBeatRule which);

/* "dissonance-figure" "mixture" "large-leap-recovery"; NULL for an invalid
   rule. */
const char *standard_counterpoint_third_species_beat_rule_name(StandardCounterpointThirdSpeciesBeatRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   an invalid exercise or an invalid rule. */
bool standard_counterpoint_third_species_beat_rule_holds(StandardCounterpointThirdSpeciesBeatRule which,
                                                         const StandardCounterpointThirdSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_third_species_beat_check(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                    unsigned *hard_violations, unsigned *soft_violations);

#endif
