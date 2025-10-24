#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_RULES_H

#include <stdbool.h>

#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * First-species independence rules for standard-counterpoint-v1: what keeps
 * the two parts distinct from each other, on top of the common rules
 * (harmonic_rules.h). The counterpoint is the part that is not the cantus
 * firmus.
 *
 *   SINGLE_REPEAT   the counterpoint repeats a note (the same pitch twice
 *                   in a row) at most once (hard)
 *   NO_REPEAT       the counterpoint repeats no note at all (soft)
 *   NO_OVERLAP      neither part leaps past the previous note of the other
 *                   part: the upper part's next note is not below the lower
 *                   part's current note, and the lower part's next note is
 *                   not above the upper part's current note (soft). This is
 *                   different from voice crossing, two parts that sound in
 *                   the wrong order at the same time
 *                   (harmonic_rules.h, NO_CROSSING)
 *   EMERGENCY_GAP   the parts are more than a tenth apart in no more than
 *                   two positions: the gap is exceeded only in
 *                   "emergencies" and only briefly (soft). The hard limit
 *                   of a twelfth is a common rule
 *
 * Every check returns false for an invalid pair or an invalid cantus part.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_SINGLE_REPEAT = 0,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_REPEAT,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_NO_OVERLAP,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_EMERGENCY_GAP,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_INDEPENDENCE_COUNT
} StandardCounterpointFirstSpeciesIndependenceRule;

#define STANDARD_COUNTERPOINT_FIRST_SPECIES_TENTH 10
#define STANDARD_COUNTERPOINT_FIRST_SPECIES_MAX_EMERGENCY_NOTES 2

const Rule *
standard_counterpoint_first_species_independence_rule(StandardCounterpointFirstSpeciesIndependenceRule which);
const Constraint *
standard_counterpoint_first_species_independence_constraint(StandardCounterpointFirstSpeciesIndependenceRule which);
/* The soft rule's Preference (scores the rule 1/1 when it holds, 0/1 when not); NULL for a hard rule. */
const Preference *standard_counterpoint_first_species_independence_preference(StandardCounterpointFirstSpeciesIndependenceRule which);

/* "single-repeat" "no-repeat" "no-overlap" "emergency-gap"; NULL for an
   invalid rule. */
const char *
standard_counterpoint_first_species_independence_rule_name(StandardCounterpointFirstSpeciesIndependenceRule which);

/* True when the texture satisfies the rule. False when it does not, and for
   invalid input. */
bool standard_counterpoint_first_species_independence_rule_holds(
    StandardCounterpointFirstSpeciesIndependenceRule which, const StandardCounterpointPair *pair,
    StandardCounterpointCantusPart cantus_part);

/* The rules the texture breaks: hard_violations has bit (1u << rule) set for
   each hard rule that fails, soft_violations for each soft rule. False
   (both untouched) for invalid input. */
bool standard_counterpoint_first_species_independence_check(const StandardCounterpointPair *pair,
                                                            StandardCounterpointCantusPart cantus_part,
                                                            unsigned *hard_violations, unsigned *soft_violations);

#endif
