#include "weighted_identification.h"

#include <math.h>

CommonPracticeWeightedConfig common_practice_weighted_config_default(void) {
    CommonPracticeWeightedConfig config;
    config.strength_factor[BEAT_STRENGTH_STRONG] = 1.0;
    config.strength_factor[BEAT_STRENGTH_MEDIUM] = 0.7;
    config.strength_factor[BEAT_STRENGTH_WEAK] = 0.4;
    config.held_factor = 0.6;
    config.bass_weight = 1.5;
    config.missing_third_penalty = 0.15;
    config.bass_chord_tone_bonus = 0.05;
    config.min_coverage = 0.5;
    return config;
}

static bool usable(double value) {
    return isfinite(value) && value >= 0.0;
}

static bool config_valid(const CommonPracticeWeightedConfig *config) {
    for (int i = 0; i < BEAT_STRENGTH_COUNT; i++) {
        if (!usable(config->strength_factor[i])) {
            return false;
        }
    }
    return usable(config->held_factor) && usable(config->bass_weight) && usable(config->missing_third_penalty) &&
           usable(config->bass_chord_tone_bonus) && usable(config->min_coverage);
}

static int pitch_class_of(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

CommonPracticeWeightedStatus common_practice_weight_segment(const CommonPracticeMember *members, size_t count,
                                                             const CommonPracticeWeightedConfig *config,
                                                             CommonPracticeWeights *out) {
    if (config == NULL || out == NULL || (members == NULL && count > 0)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    if (!config_valid(config)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG;
    }

    int bass_pitch = -1;
    bool have_bass = false;
    for (size_t i = 0; i < count; i++) {
        if (!(members[i].duration > 0.0) || !isfinite(members[i].duration) ||
            (!members[i].held && !beat_strength_valid(members[i].strength))) {
            return COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER;
        }
        if (!have_bass || members[i].pitch < bass_pitch) {
            bass_pitch = members[i].pitch;
            have_bass = true;
        }
    }

    CommonPracticeWeights weights = {{0}, 0.0, -1, -1};
    for (size_t i = 0; i < count; i++) {
        double factor = members[i].held ? config->held_factor : config->strength_factor[members[i].strength];
        double weight = members[i].duration * factor;
        if (members[i].pitch == bass_pitch) {
            weight *= config->bass_weight;
        }
        weights.pitch_class[pitch_class_of(members[i].pitch)] += weight;
        weights.total += weight;
    }
    if (have_bass) {
        weights.bass_pitch = bass_pitch;
        weights.bass_pitch_class = pitch_class_of(bass_pitch);
    }
    *out = weights;
    return COMMON_PRACTICE_WEIGHTED_OK;
}
