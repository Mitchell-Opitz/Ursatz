#include "analysis/texture/texture_analysis_input.h"

StandardTextureConfig standard_texture_config_default(void) {
    StandardTextureConfig config = {32, 256, 64, 0.75, 0.6, 0.5, 1.5, 3.0, 0.5};
    return config;
}

bool standard_texture_config_valid(const StandardTextureConfig *config) {
    return config != NULL && config->onset_grid_denominator >= 1 &&
           config->onset_tolerance_denominator >= 2 * config->onset_grid_denominator &&
           config->release_tolerance_denominator >= 2 * config->onset_grid_denominator &&
           config->rhythmic_unison_synchrony > 0.0 && config->rhythmic_unison_synchrony <= 1.0 &&
           config->independence_min > 0.0 && config->independence_min <= 1.0 && config->independence_max_synchrony > 0.0 &&
           config->independence_max_synchrony <= config->rhythmic_unison_synchrony && config->prominence_min > 1.0 &&
           config->prominence_gap_semitones >= 0.0 && config->pattern_min > 0.0 && config->pattern_min <= 1.0;
}
