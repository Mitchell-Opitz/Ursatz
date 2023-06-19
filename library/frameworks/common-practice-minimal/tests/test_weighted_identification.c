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

/* --- step 2, scoring --- (key of C major unless stated: tonic pitch class 0) */

static CommonPracticeWeights weigh(const CommonPracticeMember *members, size_t count) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, count, &config, &weights) == COMMON_PRACTICE_WEIGHTED_OK);
    return weights;
}

static void rank(const CommonPracticeWeights *weights, int tonic, CommonPracticeRankedChord out[14]) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    assert(common_practice_rank_candidates(weights, tonic, COMMON_PRACTICE_KEY_MAJOR, &config, out) ==
           COMMON_PRACTICE_WEIGHTED_OK);
}

static void assert_best(const CommonPracticeRankedChord *ranked, int degree, bool is_seventh) {
    assert(ranked[0].degree == degree && ranked[0].is_seventh == is_seventh);
}

static void test_a_pure_triad_is_covered_completely(void) {
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);
    assert_best(ranked, 1, false); /* I, and the triad beats the tied I major seventh */
    assert(ranked[0].triad == COMMON_PRACTICE_TRIAD_I);
    assert(close_to(ranked[0].score, 1.0 + 0.05)); /* all the weight, third present, bass is a chord tone */
}

static void test_the_candidates_are_the_fourteen_diatonic_chords_best_first(void) {
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);

    int seen_triad[8] = {0};
    int seen_seventh[8] = {0};
    for (int i = 0; i < 14; i++) {
        assert(ranked[i].degree >= 1 && ranked[i].degree <= 7);
        (ranked[i].is_seventh ? seen_seventh : seen_triad)[ranked[i].degree]++;
        if (i > 0) {
            assert(ranked[i - 1].score >= ranked[i].score - 1e-9);
        }
    }
    for (int d = 1; d <= 7; d++) {
        assert(seen_triad[d] == 1 && seen_seventh[d] == 1);
    }

    /* The seventh quality each degree takes in a major key. */
    for (int i = 0; i < 14; i++) {
        if (!ranked[i].is_seventh) {
            continue;
        }
        CommonPracticeSeventh expected[8] = {0,
                                             COMMON_PRACTICE_SEVENTH_MAJOR7, COMMON_PRACTICE_SEVENTH_MINOR7,
                                             COMMON_PRACTICE_SEVENTH_MINOR7, COMMON_PRACTICE_SEVENTH_MAJOR7,
                                             COMMON_PRACTICE_SEVENTH_DOMINANT7, COMMON_PRACTICE_SEVENTH_MINOR7,
                                             COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7};
        assert(ranked[i].seventh == expected[ranked[i].degree]);
    }
}

static void test_a_dominant_seventh_beats_the_dominant_triad(void) {
    CommonPracticeMember members[4] = {member(43, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(59, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(62, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(65, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 4);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);
    assert_best(ranked, 5, true); /* V7: G B D F */
    assert(ranked[0].seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7);
}

static void test_the_key_is_transposed(void) {
    /* E-flat major: Eb3 G3 Bb3 is I; the tonic is pitch class 3. */
    CommonPracticeMember members[3] = {member(51, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(58, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 3, ranked);
    assert_best(ranked, 1, false);
    assert(ranked[0].root_pitch_class == 3);
    /* The same notes in C major are no tonic chord. */
    rank(&weights, 0, ranked);
    assert(!(ranked[0].degree == 1 && !ranked[0].is_seventh));
}

static void test_a_missing_third_is_penalized(void) {
    CommonPracticeMember with_third[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                          member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                          member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeMember without_third[2] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                             member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights a = weigh(with_third, 3);
    CommonPracticeWeights b = weigh(without_third, 2);
    CommonPracticeRankedChord full[14];
    CommonPracticeRankedChord bare[14];
    rank(&a, 0, full);
    rank(&b, 0, bare);
    assert_best(bare, 1, false); /* C and G alone are still best read as I... */
    assert(close_to(full[0].score - bare[0].score, 0.15)); /* ...but cost the 0.15 penalty */
}

/* The position of a diatonic triad (is_seventh false) of the given degree in a ranking. */
static int position_of_triad(const CommonPracticeRankedChord *ranked, int degree) {
    for (int i = 0; i < 14; i++) {
        if (ranked[i].degree == degree && !ranked[i].is_seventh) {
            return i;
        }
    }
    return -1;
}

static void test_the_bass_bonus_decides_between_i_and_iii(void) {
    /* C E G B under a C bass, with the B given the weight of the C (1.5 against the bass 1.0 * 1.5), so
       the triads I (C E G) and iii (E G B) cover the same amount. The C bass is a tone of I and not of iii,
       so the bonus alone puts I ahead of iii, by exactly the bonus. (The segment also holds a whole I major
       seventh, which outranks both; the comparison is between the two triads.) */
    CommonPracticeMember members[4] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(64, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(67, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(71, false, 1.5, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 4);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);

    int i_at = position_of_triad(ranked, 1);
    int iii_at = position_of_triad(ranked, 3);
    assert(i_at >= 0 && iii_at >= 0 && i_at < iii_at);
    assert(close_to(ranked[i_at].score - ranked[iii_at].score, 0.05));
}

static void test_a_tie_goes_to_root_position(void) {
    /* The same four pitch classes with E in the bass (E 1.0 * 1.5, C 1.0, G 1.0, B 1.0): I (first
       inversion) and iii (root position) cover the same amount and both contain the bass. The tie goes to
       the chord with the bass as its root: iii ahead of I. */
    CommonPracticeMember members[4] = {member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(67, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(71, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 4);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);

    int i_at = position_of_triad(ranked, 1);
    int iii_at = position_of_triad(ranked, 3);
    assert(close_to(ranked[i_at].score, ranked[iii_at].score));
    assert(iii_at < i_at);
}

static void test_a_tie_goes_to_the_triad_over_the_seventh(void) {
    /* A C major triad: I and the I major seventh cover everything alike (the seventh's B has no weight);
       the triad is first. */
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);
    assert(!ranked[0].is_seventh && ranked[0].degree == 1);
    assert(ranked[1].is_seventh && ranked[1].degree == 1);
    assert(close_to(ranked[0].score, ranked[1].score));
}

static void test_a_second_inversion_does_not_hide_the_chord(void) {
    /* G in the bass under C and E: still I, though not in root position. */
    CommonPracticeMember members[3] = {member(43, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(64, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);
    assert_best(ranked, 1, false);
}

static void test_an_empty_segment_ranks_without_a_bass(void) {
    CommonPracticeWeights weights = weigh(NULL, 0);
    CommonPracticeRankedChord ranked[14];
    rank(&weights, 0, ranked);
    assert(ranked[0].score <= 0.0); /* nothing sounds: no chord covers anything */
}

static void test_scoring_rejects_bad_input(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeWeights weights = weigh(NULL, 0);
    CommonPracticeRankedChord ranked[14];
    assert(common_practice_rank_candidates(NULL, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked) ==
           COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MINOR, &config, ranked) ==
           COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE);
    assert(common_practice_rank_candidates(&weights, 12, COMMON_PRACTICE_KEY_MAJOR, &config, ranked) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC);
    assert(common_practice_rank_candidates(&weights, -1, COMMON_PRACTICE_KEY_MAJOR, &config, ranked) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC);
    config.bass_weight = -1.0;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);
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
    test_a_pure_triad_is_covered_completely();
    test_the_candidates_are_the_fourteen_diatonic_chords_best_first();
    test_a_dominant_seventh_beats_the_dominant_triad();
    test_the_key_is_transposed();
    test_a_missing_third_is_penalized();
    test_the_bass_bonus_decides_between_i_and_iii();
    test_a_tie_goes_to_root_position();
    test_a_tie_goes_to_the_triad_over_the_seventh();
    test_a_second_inversion_does_not_hide_the_chord();
    test_an_empty_segment_ranks_without_a_bass();
    test_scoring_rejects_bad_input();
    printf("test_weighted_identification: ok\n");
    return 0;
}
