#include "third_species_rules.h"

#include <stddef.h>

#include "cantus_firmus_rules.h"
#include "counterpoint_limits.h"
#include "melodic_rules.h"
#include "species_aggregate.h"
#include "third_species_ends_rules.h"
#include "third_species_line_rules.h"
#include "third_species_strong_rules.h"
#include "third_species_beat_rules.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT] = {
    "cantus-firmus", "spacing", "strong-beats", "other-beats", "ends", "counterpoint-line"};

const char *standard_counterpoint_third_species_group_name(StandardCounterpointThirdSpeciesGroup group) {
    return group >= 0 && group < STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT ? NAMES[group] : NULL;
}

bool standard_counterpoint_third_species_check(const StandardCounterpointThirdSpeciesInput *input,
                                                StandardCounterpointResult *out) {
    if (input == NULL || out == NULL || !standard_counterpoint_third_species_exercise_valid(input->exercise)) {
        return false;
    }
    const StandardCounterpointThirdSpeciesExercise *exercise = input->exercise;
    StandardCounterpointLimits limits = standard_counterpoint_default_limits();
    StandardCounterpointResult result = {0, 0};
    unsigned hard, soft;

    if (input->tonic != NULL) {
        if (!standard_counterpoint_cantus_check(exercise->cantus, input->tonic, &hard, &soft)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_CANTUS_FIRMUS,
                                             hard, soft);
    }
    if (!standard_counterpoint_melodic_check(exercise->cantus, &limits, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_CANTUS_FIRMUS, hard, soft);

    if (!standard_counterpoint_species_spacing_check(exercise->cantus, exercise->counterpoint,
                                                      STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR,
                                                      exercise->cantus_part, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_SPACING, hard, soft);

    if (!standard_counterpoint_third_species_strong_check(exercise, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_STRONG_BEATS, hard, soft);

    if (!standard_counterpoint_third_species_beat_check(exercise, &hard, &soft)) {
        return false;
    }
    standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_OTHER_BEATS, hard, soft);

    if (input->tonic != NULL) {
        if (!standard_counterpoint_third_species_ends_check(exercise, input->tonic, &hard)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_ENDS, hard, 0);

        if (!standard_counterpoint_third_species_line_check(exercise, input->tonic, &hard, &soft)) {
            return false;
        }
        standard_counterpoint_species_record(&result, STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNTERPOINT_LINE,
                                             hard, soft);
    }

    *out = result;
    return true;
}

bool standard_counterpoint_third_species_valid(const StandardCounterpointThirdSpeciesInput *input) {
    StandardCounterpointResult result;
    return standard_counterpoint_third_species_check(input, &result) && result.hard_violations == 0;
}
