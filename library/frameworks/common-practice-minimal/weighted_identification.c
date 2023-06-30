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

/* --- scoring --- */

#define TOLERANCE 1e-9

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};

/* The seventh quality each scale degree of a major key takes. */
static const CommonPracticeSeventh SEVENTH_OF_DEGREE[7] = {
    COMMON_PRACTICE_SEVENTH_MAJOR7,  COMMON_PRACTICE_SEVENTH_MINOR7,
    COMMON_PRACTICE_SEVENTH_MINOR7,  COMMON_PRACTICE_SEVENTH_MAJOR7,
    COMMON_PRACTICE_SEVENTH_DOMINANT7, COMMON_PRACTICE_SEVENTH_MINOR7,
    COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7};

/* True when `a` ranks ahead of `b` for a segment whose bass has pitch class bass_pitch_class. */
static bool ranks_ahead(const CommonPracticeRankedChord *a, const CommonPracticeRankedChord *b,
                        int bass_pitch_class) {
    if (fabs(a->score - b->score) > TOLERANCE) {
        return a->score > b->score;
    }
    bool a_root_position = bass_pitch_class >= 0 && a->root_pitch_class == bass_pitch_class;
    bool b_root_position = bass_pitch_class >= 0 && b->root_pitch_class == bass_pitch_class;
    if (a_root_position != b_root_position) {
        return a_root_position;
    }
    if (a->is_seventh != b->is_seventh) {
        return !a->is_seventh;
    }
    return a->degree < b->degree;
}

CommonPracticeWeightedStatus common_practice_rank_candidates(const CommonPracticeWeights *weights,
                                                              int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                              const CommonPracticeWeightedConfig *config,
                                                              CommonPracticeRankedChord out[COMMON_PRACTICE_CANDIDATE_COUNT]) {
    if (weights == NULL || config == NULL || out == NULL) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    if (!config_valid(config)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG;
    }
    if (mode != COMMON_PRACTICE_KEY_MAJOR) {
        return COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE;
    }
    if (tonic_pitch_class < 0 || tonic_pitch_class > 11) {
        return COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC;
    }

    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = 0;
    for (int degree = 0; degree < 7; degree++) {
        int root = (tonic_pitch_class + MAJOR_SCALE[degree]) % 12;
        int third = (tonic_pitch_class + MAJOR_SCALE[(degree + 2) % 7]) % 12;
        int fifth = (tonic_pitch_class + MAJOR_SCALE[(degree + 4) % 7]) % 12;
        int seventh = (tonic_pitch_class + MAJOR_SCALE[(degree + 6) % 7]) % 12;

        for (int with_seventh = 0; with_seventh < 2; with_seventh++) {
            double in_chord = weights->pitch_class[root] + weights->pitch_class[third] + weights->pitch_class[fifth];
            bool bass_is_chord_tone = weights->bass_pitch_class == root || weights->bass_pitch_class == third ||
                                      weights->bass_pitch_class == fifth;
            if (with_seventh) {
                in_chord += weights->pitch_class[seventh];
                bass_is_chord_tone = bass_is_chord_tone || weights->bass_pitch_class == seventh;
            }

            double score = weights->total > 0.0 ? in_chord / weights->total : 0.0;
            if (weights->pitch_class[third] <= 0.0) {
                score -= config->missing_third_penalty;
            }
            if (bass_is_chord_tone) {
                score += config->bass_chord_tone_bonus;
            }

            CommonPracticeRankedChord chord;
            chord.degree = degree + 1;
            chord.is_seventh = with_seventh != 0;
            chord.triad = (CommonPracticeTriad)degree;
            chord.seventh = SEVENTH_OF_DEGREE[degree];
            chord.root_pitch_class = root;
            chord.score = score;
            ranked[count++] = chord;
        }
    }

    /* Insertion sort, best first: 14 candidates, and it keeps the order stable and total. */
    for (size_t i = 1; i < count; i++) {
        CommonPracticeRankedChord moving = ranked[i];
        size_t j = i;
        while (j > 0 && ranks_ahead(&moving, &ranked[j - 1], weights->bass_pitch_class)) {
            ranked[j] = ranked[j - 1];
            j--;
        }
        ranked[j] = moving;
    }
    for (size_t i = 0; i < count; i++) {
        out[i] = ranked[i];
    }
    return COMMON_PRACTICE_WEIGHTED_OK;
}

unsigned common_practice_ranked_chord_mask(const CommonPracticeRankedChord *chord, int tonic_pitch_class) {
    if (chord == NULL || chord->degree < 1 || chord->degree > 7 || tonic_pitch_class < 0 || tonic_pitch_class > 11) {
        return 0;
    }
    int degree = chord->degree - 1;
    unsigned mask = 0;
    mask |= 1u << ((tonic_pitch_class + MAJOR_SCALE[degree]) % 12);
    mask |= 1u << ((tonic_pitch_class + MAJOR_SCALE[(degree + 2) % 7]) % 12);
    mask |= 1u << ((tonic_pitch_class + MAJOR_SCALE[(degree + 4) % 7]) % 12);
    if (chord->is_seventh) {
        mask |= 1u << ((tonic_pitch_class + MAJOR_SCALE[(degree + 6) % 7]) % 12);
    }
    return mask;
}
