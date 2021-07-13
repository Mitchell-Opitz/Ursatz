#include "first_species_line_rules.h"

#include <stddef.h>

bool standard_counterpoint_first_species_line_rule_holds(StandardCounterpointCantusRule which,
                                                         const StandardCounterpointLine *counterpoint,
                                                         const PitchSpelling *tonic) {
    if (which == STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_EXCLUDED) {
        return standard_counterpoint_line_valid(counterpoint) && tonic != NULL;
    }
    return standard_counterpoint_cantus_rule_holds(which, counterpoint, tonic);
}

bool standard_counterpoint_first_species_line_check(const StandardCounterpointLine *counterpoint,
                                                    const PitchSpelling *tonic, unsigned *hard_violations,
                                                    unsigned *soft_violations) {
    unsigned hard, soft;
    if (hard_violations == NULL || soft_violations == NULL ||
        !standard_counterpoint_cantus_check(counterpoint, tonic, &hard, &soft)) {
        return false;
    }
    unsigned excluded = 1u << STANDARD_COUNTERPOINT_FIRST_SPECIES_LINE_EXCLUDED;
    *hard_violations = hard & ~excluded;
    *soft_violations = soft & ~excluded;
    return true;
}
