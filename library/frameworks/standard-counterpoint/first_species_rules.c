#include "first_species_rules.h"

#include <stddef.h>

#include "first_species_ends_rules.h"
#include "first_species_harmony_rules.h"
#include "first_species_independence_rules.h"
#include "first_species_line_rules.h"
#include "first_species_motion_rules.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT] = {
    "cantus-firmus", "melodic", "harmonic", "tendency", "counterpoint-line",
    "harmony",       "ends",    "independence", "motion"};

const char *standard_counterpoint_first_species_group_name(StandardCounterpointFirstSpeciesGroup group) {
    return group >= 0 && group < STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT ? NAMES[group] : NULL;
}

static void record(StandardCounterpointResult *result, StandardCounterpointFirstSpeciesGroup group, unsigned hard,
                   unsigned soft) {
    if (hard != 0) {
        result->hard_violations |= 1u << group;
    }
    if (soft != 0) {
        result->soft_violations |= 1u << group;
    }
}

bool standard_counterpoint_first_species_check(const StandardCounterpointInput *input,
                                               StandardCounterpointResult *out) {
    StandardCounterpointResult result;
    if (out == NULL || !standard_counterpoint_check(input, &result)) {
        return false;
    }

    const StandardCounterpointPair *pair = input->texture;
    const StandardCounterpointLine *counterpoint =
        input->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER ? pair->upper : pair->lower;
    unsigned hard, soft;

    if (input->tonic != NULL) {
        if (!standard_counterpoint_first_species_line_check(counterpoint, input->tonic, &hard, &soft)) {
            return false;
        }
        record(&result, STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNTERPOINT_LINE, hard, soft);

        if (!standard_counterpoint_first_species_ends_check(pair, input->cantus_part, input->tonic, &hard)) {
            return false;
        }
        record(&result, STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_ENDS, hard, 0);
    }

    if (!standard_counterpoint_first_species_harmony_check(pair, &hard, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONY, hard, soft);

    if (!standard_counterpoint_first_species_independence_check(pair, input->cantus_part, &hard, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_INDEPENDENCE, hard, soft);

    if (!standard_counterpoint_first_species_motion_check(pair, &soft)) {
        return false;
    }
    record(&result, STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_MOTION, 0, soft);

    *out = result;
    return true;
}

bool standard_counterpoint_first_species_valid(const StandardCounterpointInput *input) {
    StandardCounterpointResult result;
    return standard_counterpoint_first_species_check(input, &result) && result.hard_violations == 0;
}
