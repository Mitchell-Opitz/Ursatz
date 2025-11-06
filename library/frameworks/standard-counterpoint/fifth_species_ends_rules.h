#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_RULES_H

#include <stdbool.h>

#include "fifth_species_exercise.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Fifth-species beginning and ending rules for standard-counterpoint-v1. They
 * need the tonic ("do") of the mode; which part is the cantus firmus is part
 * of the exercise. A fifth-species exercise may begin with a rest of one, two,
 * or three quarters (the exercise shape, fifth_species_exercise.h).
 *
 *   BEGIN          the first note of the counterpoint is a perfect
 *                  consonance against the cantus firmus: a unison, a fifth,
 *                  or an octave (hard)
 *   CLAUSULA_VERA  the last two notes of the exercise make a clausula vera:
 *                  the cantus firmus steps into its last note, and the note
 *                  before the last bar of the counterpoint steps the other
 *                  way into its last note, with a third or sixth against the
 *                  cantus firmus before the last bar (hard). The note before
 *                  the last bar is the one that sounds just before it, a
 *                  suspension resolution included
 *   FINAL_BAR      the last bar is a whole note on do (hard)
 *
 * Notes are compared by letter and accidental for the tonic, octave ignored.
 * Every check returns false for an invalid exercise or a NULL tonic.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_BEGIN = 0,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_CLAUSULA_VERA,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_FINAL_BAR,
    STANDARD_COUNTERPOINT_FIFTH_SPECIES_ENDS_COUNT
} StandardCounterpointFifthSpeciesEndsRule;

const Rule *standard_counterpoint_fifth_species_ends_rule(StandardCounterpointFifthSpeciesEndsRule which);
const Constraint *standard_counterpoint_fifth_species_ends_constraint(StandardCounterpointFifthSpeciesEndsRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_fifth_species_ends_preference(StandardCounterpointFifthSpeciesEndsRule which);

/* "begin" "clausula-vera" "final-bar"; NULL for an invalid rule. */
const char *standard_counterpoint_fifth_species_ends_rule_name(StandardCounterpointFifthSpeciesEndsRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   invalid input. */
bool standard_counterpoint_fifth_species_ends_rule_holds(StandardCounterpointFifthSpeciesEndsRule which,
                                                         const StandardCounterpointFifthSpeciesExercise *exercise,
                                                         const PitchSpelling *tonic);

/* The rules the exercise breaks, as a bit mask: bit (1u << rule) is set for each
   that fails. All three are hard. False (*violations untouched) for invalid
   input. */
bool standard_counterpoint_fifth_species_ends_check(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                    const PitchSpelling *tonic, unsigned *violations);

#endif
