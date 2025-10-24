#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"
#include "second_species_exercise.h"

/*
 * Second-species beginning and ending rules for standard-counterpoint-v1:
 * how the counterpoint starts and finishes, which follows first species
 * (first_species_ends_rules.h) with the rhythm of two notes to a bar. They
 * need the tonic ("do") of the mode; which part is the cantus firmus is
 * part of the exercise.
 *
 *   BEGIN         the first note of the counterpoint is do or sol above the
 *                 cantus firmus, do below it (hard)
 *   OPENING_BAR   the first bar is two half notes, or a half rest followed
 *                 by a half note; it is not a whole note (hard)
 *   FINAL_BAR     the last bar is a whole note on do: a note on the strong
 *                 beat tied through the weak beat (hard)
 *   CLAUSULA_VERA the note before the final do, which is the weak beat of
 *                 the penultimate bar when it is two half notes and the
 *                 whole note when it is one, reaches the final do by step
 *                 in contrary motion to the cantus firmus (ti to do against
 *                 re to do, or the reverse), and makes a major or minor
 *                 third or sixth with the cantus firmus note it sounds
 *                 with (hard)
 *
 * Notes are compared by letter and accidental, octave ignored. Every check
 * returns false for an invalid exercise or a NULL tonic.
 */

typedef enum {
    STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_BEGIN = 0,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_OPENING_BAR,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_FINAL_BAR,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_CLAUSULA_VERA,
    STANDARD_COUNTERPOINT_SECOND_SPECIES_ENDS_COUNT
} StandardCounterpointSecondSpeciesEndsRule;

const Rule *standard_counterpoint_second_species_ends_rule(StandardCounterpointSecondSpeciesEndsRule which);
const Constraint *standard_counterpoint_second_species_ends_constraint(StandardCounterpointSecondSpeciesEndsRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_second_species_ends_preference(StandardCounterpointSecondSpeciesEndsRule which);

/* "begin" "opening-bar" "final-bar" "clausula-vera"; NULL for an invalid
   rule. */
const char *standard_counterpoint_second_species_ends_rule_name(StandardCounterpointSecondSpeciesEndsRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   invalid input. */
bool standard_counterpoint_second_species_ends_rule_holds(StandardCounterpointSecondSpeciesEndsRule which,
                                                          const StandardCounterpointSecondSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic);

/* The rules the exercise breaks, as a bit mask: bit (1u << rule) is set for
   each that fails. All four are hard. False (*violations untouched) for
   invalid input. */
bool standard_counterpoint_second_species_ends_check(const StandardCounterpointSecondSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *violations);

#endif
