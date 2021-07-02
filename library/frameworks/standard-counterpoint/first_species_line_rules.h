#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_RULES_H

#include <stdbool.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_line.h"
#include "pitch/pitch_spelling.h"

/*
 * First-species counterpoint line rules for standard-counterpoint-v1. The
 * counterpoint line follows the principles of a good cantus firmus
 * (cantus_firmus_rules.h): a first-species counterpoint is two lines of
 * cantus firmus quality. The cantus firmus rules apply to the counterpoint
 * unchanged except one:
 *
 *   TONIC_ENDS   not applied. The counterpoint may begin on sol above the
 *                cantus firmus (first_species_ends_rules.h gives the rules
 *                for how the counterpoint begins and ends)
 *
 * The line takes the hard and soft status each rule has in the cantus
 * firmus group. The result is reported in the cantus firmus rule enum
 * (StandardCounterpointCantusRule), with the TONIC_ENDS bit never set.
 *
 * Every check returns false for an invalid line or a NULL tonic.
 */

/* The cantus firmus rule that does not apply to a counterpoint line. */
#define STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_EXCLUDED STANDARD_COUNTERPOINT_CANTUS_RULE_TONIC_ENDS

/* True when the counterpoint line satisfies the cantus firmus rule `which`.
   True for the excluded rule. False when it does not, and for invalid
   input. */
bool standard_counterpoint_first_species_line_rule_holds(StandardCounterpointCantusRule which,
                                                         const StandardCounterpointLine *counterpoint,
                                                         const PitchSpelling *tonic);

/* The rules a counterpoint line breaks: hard_violations has bit
   (1u << rule) set for each hard cantus firmus rule that fails,
   soft_violations for each soft one. False (both untouched) for invalid
   input. */
bool standard_counterpoint_first_species_line_check(const StandardCounterpointLine *counterpoint,
                                                    const PitchSpelling *tonic, unsigned *hard_violations,
                                                    unsigned *soft_violations);

#endif
