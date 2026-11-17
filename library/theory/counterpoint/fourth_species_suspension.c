#include "theory/counterpoint/fourth_species_suspension.h"

#include <stddef.h>

#include "theory/counterpoint/consonance_rules.h"
#include "theory/tonality/interval.h"
#include "model/pitch/pitch_arithmetic.h"

bool standard_counterpoint_suspension_is_dissonant(StandardCounterpointSuspensionType type) {
    return type >= 0 && type < STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE;
}

static bool consonant(const StandardTonalityInterval *interval) {
    StandardTonalityIntervalCategory category;
    return standard_counterpoint_classify_interval(interval, &category) &&
           category != STANDARD_COUNTERPOINT_DISSONANCE;
}

bool standard_counterpoint_fourth_species_suspension(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     size_t bar, StandardCounterpointSuspensionType *type) {
    if (type == NULL || !standard_counterpoint_fourth_species_exercise_valid(exercise) || bar == 0 ||
        bar + 1 >= exercise->cantus->count || !standard_counterpoint_fourth_species_downbeat_is_tie(exercise, bar)) {
        return false;
    }
    const PitchSpelling *held = standard_counterpoint_fourth_species_downbeat(exercise, bar);
    const PitchSpelling *resolution = standard_counterpoint_fourth_species_weak(exercise, bar);
    const PitchSpelling *cantus = standard_counterpoint_fourth_species_cantus(exercise, bar);

    StandardTonalityInterval preparation, suspension, resolved, step;
    if (!standard_counterpoint_fourth_species_weak_interval(exercise, bar - 1, &preparation) ||
        !standard_counterpoint_fourth_species_downbeat_interval(exercise, bar, &suspension) ||
        !standard_counterpoint_fourth_species_weak_interval(exercise, bar, &resolved) ||
        !standard_tonality_interval_between(held, resolution, &step)) {
        return false;
    }
    bool steps_down = standard_tonality_interval_is_step(&step) &&
                      standard_counterpoint_interval_melodic_consonant(&step) &&
                      pitch_spelling_direction(held, resolution) < 0;
    if (!consonant(&preparation) || !consonant(&resolved) || !steps_down) {
        return false;
    }

    if (consonant(&suspension)) {
        *type = STANDARD_COUNTERPOINT_SUSPENSION_CONSONANT_TIE;
        return true;
    }

    bool above = pitch_spelling_direction(cantus, held) > 0;
    int from = suspension.simple_size;
    int to = resolved.simple_size;
    if (above) {
        if (from == 7 && to == 6) {
            *type = STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX;
        } else if (from == 4 && suspension.quality == STANDARD_TONALITY_INTERVAL_QUALITY_PERFECT && to == 3) {
            *type = STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE;
        } else if (from == 2 && (to == 1 || to == 8)) {
            *type = STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT;
        } else {
            return false;
        }
        return true;
    }
    if (from == 2 && to == 3) {
        *type = STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE;
        return true;
    }
    return false;
}
