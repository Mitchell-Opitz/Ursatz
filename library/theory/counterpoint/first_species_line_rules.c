#include "theory/counterpoint/first_species_line_rules.h"

#include <stddef.h>

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
