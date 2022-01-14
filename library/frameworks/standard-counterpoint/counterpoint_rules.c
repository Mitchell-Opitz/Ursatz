#include "counterpoint_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "harmonic_rules.h"
#include "melodic_rules.h"
#include "species_aggregate.h"
#include "tendency_rules.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_GROUP_COUNT] = {"cantus-firmus", "melodic", "harmonic",
                                                                      "tendency"};

const char *standard_counterpoint_group_name(StandardCounterpointGroup group) {
    return group >= 0 && group < STANDARD_COUNTERPOINT_GROUP_COUNT ? NAMES[group] : NULL;
}

bool standard_counterpoint_check(const StandardCounterpointInput *input, StandardCounterpointResult *out) {
    if (input == NULL || out == NULL || !standard_counterpoint_pair_valid(input->texture) ||
        (input->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_LOWER &&
         input->cantus_part != STANDARD_COUNTERPOINT_CANTUS_IN_UPPER)) {
        return false;
    }

    StandardCounterpointLimits defaults = standard_counterpoint_default_limits();
    const StandardCounterpointLimits *limits = input->limits != NULL ? input->limits : &defaults;
    const StandardCounterpointLine *lines[2] = {input->texture->lower, input->texture->upper};
    const StandardCounterpointLine *cantus =
        input->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER ? input->texture->lower : input->texture->upper;

    StandardCounterpointResult result = {0, 0};
    unsigned hard, soft;

    if (input->tonic != NULL) {
        if (!standard_counterpoint_cantus_check(cantus, input->tonic, &hard, &soft)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS, hard, soft);
    }

    for (int i = 0; i < 2; i++) {
        unsigned line_hard, line_soft, tendency;
        if (!standard_counterpoint_melodic_check(lines[i], limits, &line_hard, &line_soft) ||
            !standard_counterpoint_tendency_check(lines[i], &tendency)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_GROUP_MELODIC, line_hard, line_soft);
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_GROUP_TENDENCY, 0, tendency);
    }

    if (!standard_counterpoint_harmonic_check(input->texture, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_GROUP_HARMONIC, hard, soft);

    *out = result;
    return true;
}

bool standard_counterpoint_valid(const StandardCounterpointInput *input) {
    StandardCounterpointResult result;
    return standard_counterpoint_check(input, &result) && result.hard_violations == 0;
}
