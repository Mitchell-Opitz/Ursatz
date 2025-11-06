#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULES_H

#include <stdbool.h>

#include "fifth_species_exercise.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Fifth-species rhythm rules for standard-counterpoint-v1. Fifth species mixes
 * the note values of the first four species, and adds eighth notes, so the
 * rules are about how the values are used (fifth_species_pattern.h). The
 * shape of every eighth pair, two notes on the second or fourth beat, is part
 * of what a fifth-species exercise is (fifth_species_exercise.h), and the last
 * bar is a whole note.
 *
 *   EIGHTHS_STEPWISE  an eighth note is approached and left by step or by
 *                     repetition of the pitch (the anticipation repeats it):
 *                     no leap to or from an eighth (hard)
 *   MIXTURE           an exercise of at least six bars uses at least three of
 *                     the five note values (soft)
 *   NO_LONG_RUN       no more than three bars in a row have the same pattern;
 *                     a run of syncopated bars, the chain of suspensions, is
 *                     not limited (soft)
 *   EIGHTHS_SPARING   eighth notes are used sparingly: no two bars in a row
 *                     have a pair of eighths (soft)
 *
 * Every check returns false for an invalid exercise.
 */

#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_MIN_BARS 6
#define STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_MAX_RUN 3

typedef enum {
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_EIGHTHS_STEPWISE = 0,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_MIXTURE,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_NO_LONG_RUN,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_EIGHTHS_SPARING,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_RHYTHM_RULE_COUNT
} StandardCounterpointFifthSpeciesRhythmRule;

const Rule *standard_counterpoint_fifth_species_rhythm_rule(StandardCounterpointFifthSpeciesRhythmRule which);
const Constraint *
standard_counterpoint_fifth_species_rhythm_constraint(StandardCounterpointFifthSpeciesRhythmRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_fifth_species_rhythm_preference(StandardCounterpointFifthSpeciesRhythmRule which);

/* "eighths-stepwise" "mixture" "no-long-run" "eighths-sparing"; NULL for an invalid rule. */
const char *
standard_counterpoint_fifth_species_rhythm_rule_name(StandardCounterpointFifthSpeciesRhythmRule which);

/* True when the exercise satisfies the rule. False when it does not, and for an
   invalid exercise or an invalid rule. */
bool standard_counterpoint_fifth_species_rhythm_rule_holds(
    StandardCounterpointFifthSpeciesRhythmRule which, const StandardCounterpointFifthSpeciesExercise *exercise);

/* The rules the exercise breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False (both
   untouched) for an invalid exercise. */
bool standard_counterpoint_fifth_species_rhythm_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                           unsigned *hard_violations, unsigned *soft_violations);

#endif
