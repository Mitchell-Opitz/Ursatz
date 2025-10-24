#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_RULES_H

#include <stdbool.h>

#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * First-species beginning and ending rules for standard-counterpoint-v1:
 * how the counterpoint starts and finishes against the cantus firmus. They
 * need the tonic ("do") of the mode and which part is the cantus firmus;
 * the counterpoint is the other part.
 *
 *   BEGIN                      the counterpoint begins on a perfect
 *                              consonance that keeps the tonal context
 *                              clear: above the cantus firmus on do or
 *                              sol (a P1, P5, or P8 above the cantus),
 *                              below it on do only (P1 or P8 below; sol
 *                              would be a dissonant fourth, fa a fifth
 *                              that sounds like do and sol) (hard)
 *   END_ON_TONIC               the counterpoint ends on do (hard)
 *   CONTRARY_STEPWISE_APPROACH both parts reach the last position by
 *                              step, in contrary motion: ti to do against
 *                              re to do, or the reverse (hard)
 *   PENULTIMATE_THIRD_OR_SIXTH the interval before the last is a major or
 *                              minor third or sixth (hard)
 *
 * The last two rules together are the clausula vera. Notes are compared by
 * letter and accidental, octave ignored. Every check returns false for an
 * invalid pair, a NULL tonic, or an invalid cantus part.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_BEGIN = 0,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_END_ON_TONIC,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_CONTRARY_STEPWISE_APPROACH,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_PENULTIMATE_THIRD_OR_SIXTH,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_ENDS_COUNT
} StandardCounterpointFirstSpeciesEndsRule;

const Rule *standard_counterpoint_first_species_ends_rule(StandardCounterpointFirstSpeciesEndsRule which);
const Constraint *standard_counterpoint_first_species_ends_constraint(StandardCounterpointFirstSpeciesEndsRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_first_species_ends_preference(StandardCounterpointFirstSpeciesEndsRule which);

/* "begin" "end-on-tonic" "contrary-stepwise-approach"
   "penultimate-third-or-sixth"; NULL for an invalid rule. */
const char *standard_counterpoint_first_species_ends_rule_name(StandardCounterpointFirstSpeciesEndsRule which);

/* True when the texture satisfies the rule. False when it does not, and for
   invalid input. */
bool standard_counterpoint_first_species_ends_rule_holds(StandardCounterpointFirstSpeciesEndsRule which,
                                                         const StandardCounterpointPair *pair,
                                                         StandardCounterpointCantusPart cantus_part,
                                                         const PitchSpelling *tonic);

/* The rules the texture breaks, as a bit mask: bit (1u << rule) is set for
   each that fails. All four are hard. False (*violations untouched) for
   invalid input. */
bool standard_counterpoint_first_species_ends_check(const StandardCounterpointPair *pair,
                                                    StandardCounterpointCantusPart cantus_part,
                                                    const PitchSpelling *tonic, unsigned *violations);

#endif
