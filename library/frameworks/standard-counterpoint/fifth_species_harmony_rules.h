#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULES_H

#include <stdbool.h>

#include "fifth_species_exercise.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Fifth-species strong-beat and harmony rules for standard-counterpoint-v1:
 * what the counterpoint sounds against the cantus firmus, and how the
 * downbeats follow one another. The downbeat of a bar is the sounding of the
 * counterpoint on its first eighth, a new note or a tie, and the downbeats of
 * the bars are read in order (a bar whose downbeat is a rest has none). The
 * perfect intervals are the fifth and the octave, and a unison counts as an
 * octave; compound intervals count as simple. The dissonance figures are those
 * of fifth_species_figure.h.
 *
 *   DOWNBEAT_CONSONANT   a new note on a downbeat is a consonance against the
 *                        cantus firmus (hard)
 *   DISSONANCE_FIGURE    every dissonant new note, and every dissonant tie of
 *                        a downbeat, is part of a figure (hard)
 *   NO_REPEATED_PERFECT  two downbeats in a row are not the same perfect
 *                        interval (hard)
 *   NO_DIRECT_PERFECT    a downbeat that is a perfect interval is not reached
 *                        from the note before it by similar or parallel
 *                        motion with the cantus firmus (hard)
 *   MAX_SAME_IMPERFECT_RUN  no more than three downbeats in a row are the same
 *                        imperfect consonance (hard)
 *   NO_INTERIOR_UNISON   no downbeat is a unison except the first and the last
 *                        (soft)
 *
 * Every check returns false for an invalid exercise.
 */

#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_MAX_IMPERFECT_RUN 3

typedef enum {
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_DOWNBEAT_CONSONANT = 0,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_DISSONANCE_FIGURE,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_REPEATED_PERFECT,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_DIRECT_PERFECT,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_MAX_SAME_IMPERFECT_RUN,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_NO_INTERIOR_UNISON,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_HARMONY_RULE_COUNT
} StandardCounterpointFifthSpeciesHarmonyRule;

const Rule *standard_counterpoint_fifth_species_harmony_rule(StandardCounterpointFifthSpeciesHarmonyRule which);
const Constraint *
standard_counterpoint_fifth_species_harmony_constraint(StandardCounterpointFifthSpeciesHarmonyRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_fifth_species_harmony_preference(StandardCounterpointFifthSpeciesHarmonyRule which);

/* "downbeat-consonant" "dissonance-figure" "no-repeated-perfect"
   "no-direct-perfect" "max-same-imperfect-run" "no-interior-unison"; NULL for an
   invalid rule. */
const char *standard_counterpoint_fifth_species_harmony_rule_name(StandardCounterpointFifthSpeciesHarmonyRule which);

/* True when the exercise satisfies the rule. False when it does not, and for an
   invalid exercise or an invalid rule. */
bool standard_counterpoint_fifth_species_harmony_rule_holds(
    StandardCounterpointFifthSpeciesHarmonyRule which, const StandardCounterpointFifthSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_fifth_species_harmony_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                       unsigned *hard_violations, unsigned *soft_violations);

#endif
