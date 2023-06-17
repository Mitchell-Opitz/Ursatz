#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "weighted_identification.h"

/*
 * Weighted chord identification tests. Step 1, the weights: duration sounding in the segment times the
 * beat-strength factor of the attack (or the held factor), the bass multiplied by the bass weight, and
 * a weight per pitch class.
 */

static bool close_to(double a, double b) {
    return fabs(a - b) < 1e-12;
}

static CommonPracticeMember member(int pitch, bool held, double duration, BeatStrength strength) {
    CommonPracticeMember m;
    memset(&m, 0, sizeof(m));
    m.pitch = pitch;
    m.held = held;
    m.duration = duration;
    m.strength = strength;
    return m;
}

static void test_default_config(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    assert(close_to(config.strength_factor[BEAT_STRENGTH_STRONG], 1.0));
    assert(close_to(config.strength_factor[BEAT_STRENGTH_MEDIUM], 0.7));
    assert(close_to(config.strength_factor[BEAT_STRENGTH_WEAK], 0.4));
    assert(close_to(config.held_factor, 0.6));
    assert(close_to(config.bass_weight, 1.5));
    assert(close_to(config.missing_third_penalty, 0.15));
    assert(close_to(config.bass_chord_tone_bonus, 0.05));
    assert(close_to(config.min_coverage, 0.5));
}

static void test_a_pure_triad_weighs_its_three_pitch_classes(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 3, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[0], 1.5)); /* C3 is the bass: 1.0 * 1.0 * 1.5 */
    assert(close_to(weights.pitch_class[4], 1.0));
    assert(close_to(weights.pitch_class[7], 1.0));
    assert(close_to(weights.pitch_class[2], 0.0));
    assert(close_to(weights.total, 3.5));
    assert(weights.bass_pitch == 48 && weights.bass_pitch_class == 0);
}

static void test_duration_and_beat_strength_scale_a_member(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember members[3] = {member(60, false, 0.5, BEAT_STRENGTH_MEDIUM),
                                       member(64, false, 0.25, BEAT_STRENGTH_WEAK),
                                       member(48, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 3, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[0], 0.5 * 0.7 + 1.0 * 1.0 * 1.5)); /* C4 medium, plus the C3 bass */
    assert(close_to(weights.pitch_class[4], 0.25 * 0.4));
    assert(weights.bass_pitch == 48);
}

static void test_a_held_member_uses_the_held_factor_not_its_attack_strength(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    /* The strength of a held member is not read: an invalid one is fine. */
    CommonPracticeMember held = member(55, true, 1.0, BEAT_STRENGTH_COUNT);
    CommonPracticeMember attacked = member(64, false, 1.0, BEAT_STRENGTH_WEAK);
    CommonPracticeMember members[2] = {held, attacked};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 2, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[7], 0.6 * 1.5)); /* G3 is the lowest: held factor, then the bass weight */
    assert(close_to(weights.pitch_class[4], 0.4));
}

static void test_every_member_at_the_bass_pitch_gets_the_bass_weight(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(48, true, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 3, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    /* Two C3s (one attacked, one held) at the bass, and a C4 above: pitch class C sums all three. */
    assert(close_to(weights.pitch_class[0], 1.0 * 1.5 + 0.6 * 1.5 + 1.0));
}

static void test_the_bass_is_the_lowest_pitch_not_the_lowest_pitch_class(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember members[2] = {member(71, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 2, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.bass_pitch == 60 && weights.bass_pitch_class == 0); /* C4, though B (11) is a higher class */
}

static void test_a_configured_factor_changes_the_weights(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.bass_weight = 2.0;
    config.held_factor = 1.0;
    CommonPracticeMember members[2] = {member(48, true, 1.0, BEAT_STRENGTH_WEAK),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 2, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[0], 2.0));
    assert(close_to(weights.pitch_class[4], 1.0));
}

static void test_the_result_is_deterministic(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember members[4] = {member(48, false, 0.3, BEAT_STRENGTH_STRONG),
                                       member(64, true, 0.7, BEAT_STRENGTH_WEAK),
                                       member(67, false, 0.1, BEAT_STRENGTH_MEDIUM),
                                       member(72, false, 0.45, BEAT_STRENGTH_WEAK)};
    CommonPracticeWeights a;
    CommonPracticeWeights b;
    assert(common_practice_weight_segment(members, 4, &config, &a) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_weight_segment(members, 4, &config, &b) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
}

static void test_no_members_weigh_nothing(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(NULL, 0, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.total == 0.0 && weights.bass_pitch == -1 && weights.bass_pitch_class == -1);
}

static void test_rejects_bad_input_and_leaves_the_output_untouched(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember good = member(60, false, 1.0, BEAT_STRENGTH_STRONG);
    CommonPracticeWeights weights;
    memset(&weights, 0x7f, sizeof(weights));
    CommonPracticeWeights before = weights;

    assert(common_practice_weight_segment(&good, 1, NULL, &weights) == COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_weight_segment(&good, 1, &config, NULL) == COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_weight_segment(NULL, 2, &config, &weights) == COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);

    CommonPracticeWeightedConfig negative = config;
    negative.held_factor = -0.1;
    assert(common_practice_weight_segment(&good, 1, &negative, &weights) == COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);
    CommonPracticeWeightedConfig not_finite = config;
    not_finite.bass_weight = INFINITY;
    assert(common_practice_weight_segment(&good, 1, &not_finite, &weights) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);

    CommonPracticeMember empty_duration = member(60, false, 0.0, BEAT_STRENGTH_STRONG);
    assert(common_practice_weight_segment(&empty_duration, 1, &config, &weights) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER);
    CommonPracticeMember bad_strength = member(60, false, 1.0, BEAT_STRENGTH_COUNT);
    assert(common_practice_weight_segment(&bad_strength, 1, &config, &weights) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER);

    assert(memcmp(&weights, &before, sizeof(weights)) == 0);
}

int main(void) {
    test_default_config();
    test_a_pure_triad_weighs_its_three_pitch_classes();
    test_duration_and_beat_strength_scale_a_member();
    test_a_held_member_uses_the_held_factor_not_its_attack_strength();
    test_every_member_at_the_bass_pitch_gets_the_bass_weight();
    test_the_bass_is_the_lowest_pitch_not_the_lowest_pitch_class();
    test_a_configured_factor_changes_the_weights();
    test_the_result_is_deterministic();
    test_no_members_weigh_nothing();
    test_rejects_bad_input_and_leaves_the_output_untouched();
    printf("test_weighted_identification: ok\n");
    return 0;
}
