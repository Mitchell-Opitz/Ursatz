#include "second_species_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_limits.h"
#include "melodic_rules.h"
#include "species_aggregate.h"
#include "second_species_ends_rules.h"
#include "second_species_line_rules.h"
#include "second_species_strong_rules.h"
#include "second_species_weak_rules.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_COUNT] = {
    "cantus-firmus", "spacing", "strong-beats", "weak-beats", "ends", "counterpoint-line"};

const char *standard_counterpoint_second_species_group_name(StandardCounterpointSecondSpeciesGroup group) {
    return group >= 0 && group < STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_COUNT ? NAMES[group] : NULL;
}

bool standard_counterpoint_second_species_check(const StandardCounterpointSecondSpeciesInput *input,
                                                StandardCounterpointResult *out) {
    if (input == NULL || out == NULL || !standard_counterpoint_second_species_exercise_valid(input->exercise)) {
        return false;
    }
    const StandardCounterpointSecondSpeciesExercise *exercise = input->exercise;
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    StandardCounterpointResult result = {0, 0};
    unsigned hard, soft;

    if (input->tonic != NULL) {
        if (!standard_counterpoint_cantus_check(exercise->cantus, input->tonic, &hard, &soft)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_CANTUS_FIRMUS,
                                             hard, soft);
    }
    if (!standard_counterpoint_melodic_check(exercise->cantus, &limits, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_CANTUS_FIRMUS, hard, soft);

    if (!standard_counterpoint_species_spacing_check(exercise->cantus, exercise->counterpoint,
                                                      STANDARD_COUNTERPOINT_SECOND_SPECIES_SLOTS_PER_BAR,
                                                      exercise->cantus_part, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_SPACING, hard, soft);

    if (!standard_counterpoint_second_species_strong_check(exercise, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_STRONG_BEATS, hard, soft);

    if (!standard_counterpoint_second_species_weak_check(exercise, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_WEAK_BEATS, hard, soft);

    if (input->tonic != NULL) {
        if (!standard_counterpoint_second_species_ends_check(exercise, input->tonic, &hard)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_ENDS, hard, 0);

        if (!standard_counterpoint_second_species_line_check(exercise, input->tonic, &hard, &soft)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_COUNTERPOINT_LINE,
                                             hard, soft);
    }

    *out = result;
    return true;
}

bool standard_counterpoint_second_species_valid(const StandardCounterpointSecondSpeciesInput *input) {
    StandardCounterpointResult result;
    return standard_counterpoint_second_species_check(input, &result) && result.hard_violations == 0;
}
