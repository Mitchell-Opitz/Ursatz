#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_RULES_H

#include <stdbool.h>

#include "fourth_species_exercise.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Fourth-species beginning and ending rules for standard-counterpoint-v1.
 * They need the tonic ("do") of the mode; which part is the cantus firmus is
 * part of the exercise. Every fourth-species exercise begins with a half rest
 * (the exercise shape, fourth_species_exercise.h).
 *
 *   BEGIN                the first note of the counterpoint, on the weak beat
 *                        of the first bar, is do or sol above the cantus
 *                        firmus, do below it (hard)
 *   CANTUS_ENDS_RE_DO    the cantus firmus ends re to do: the supertonic, a
 *                        step above the tonic, then the tonic. A cantus firmus
 *                        that ends ti to do is not used (hard)
 *   PENULTIMATE_SUSPENSION  the penultimate bar holds do then ti, and do is a
 *                        dissonant suspension tied over from the bar before:
 *                        a 7-6 suspension above the cantus firmus' re, or a
 *                        2-3 suspension below it (hard)
 *   FINAL_BAR            the last bar is a whole note on do (hard)
 *
 * Notes are compared by letter and accidental, octave ignored. Every check
 * returns false for an invalid exercise or a NULL tonic.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_BEGIN = 0,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_CANTUS_ENDS_RE_DO,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_PENULTIMATE_SUSPENSION,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_FINAL_BAR,
    STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT
} StandardCounterpointFourthSpeciesEndsRule;

const Rule *standard_counterpoint_fourth_species_ends_rule(StandardCounterpointFourthSpeciesEndsRule which);
const Constraint *standard_counterpoint_fourth_species_ends_constraint(StandardCounterpointFourthSpeciesEndsRule which);

/* "begin" "cantus-ends-re-do" "penultimate-suspension" "final-bar"; NULL for an
   invalid rule. */
const char *standard_counterpoint_fourth_species_ends_rule_name(StandardCounterpointFourthSpeciesEndsRule which);

/* True when the exercise satisfies the rule. False when it does not, and for
   invalid input. */
bool standard_counterpoint_fourth_species_ends_rule_holds(StandardCounterpointFourthSpeciesEndsRule which,
                                                          const StandardCounterpointFourthSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic);

/* The rules the exercise breaks, as a bit mask: bit (1u << rule) is set for each
   that fails. All four are hard. False (*violations untouched) for invalid
   input. */
bool standard_counterpoint_fourth_species_ends_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *violations);

#endif
