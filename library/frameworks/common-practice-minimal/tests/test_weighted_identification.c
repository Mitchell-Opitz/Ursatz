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

/* The defaults with the bass weighted 1.5, so the bass factor shows in the sums (the default is 1.1). */
static CommonPracticeWeightedConfig bass_heavy(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.bass_weight = 1.5;
    return config;
}

static void test_default_config(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    assert(close_to(config.strength_factor[BEAT_STRENGTH_STRONG], 1.0));
    assert(close_to(config.strength_factor[BEAT_STRENGTH_MEDIUM], 0.7));
    assert(close_to(config.strength_factor[BEAT_STRENGTH_WEAK], 0.4));
    assert(close_to(config.held_factor, 0.6));
    assert(close_to(config.bass_weight, 1.1));
    assert(close_to(config.missing_third_penalty, 0.15));
    assert(close_to(config.bass_chord_tone_bonus, 0.05));
    assert(config.seventh_penalty == 0.0);
    assert(close_to(config.min_coverage, 0.5));
    assert(!config.require_bass_chord_tone);
    assert(!config.applied_chords);
}

static void test_a_pure_triad_weighs_its_three_pitch_classes(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 3, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[0], 1.5)); /* C3 is the bass: 1.0 * 1.0 * 1.5 */
    assert(close_to(weights.pitch_class[4], 1.0));
    assert(close_to(weights.pitch_class[7], 1.0));
    assert(close_to(weights.pitch_class[2], 0.0));
    assert(close_to(weights.total, 3.5));
    assert(weights.bass_pitch == 48 && weights.bass_pitch_class == 0);
}

static void test_duration_and_beat_strength_scale_a_member(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[3] = {member(60, false, 0.5, BEAT_STRENGTH_MEDIUM),
                                       member(64, false, 0.25, BEAT_STRENGTH_WEAK),
                                       member(48, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 3, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[0], (0.5 * 0.7 + 1.0 * 1.0) * 1.5)); /* C4 medium and the C3 bass, both on the bass pitch class */
    assert(close_to(weights.pitch_class[4], 0.25 * 0.4));
    assert(weights.bass_pitch == 48);
}

static void test_a_held_member_uses_the_held_factor_not_its_attack_strength(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    /* The strength of a held member is not read: an invalid one is fine. */
    CommonPracticeMember held = member(55, true, 1.0, BEAT_STRENGTH_COUNT);
    CommonPracticeMember attacked = member(64, false, 1.0, BEAT_STRENGTH_WEAK);
    CommonPracticeMember members[2] = {held, attacked};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 2, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[7], 0.6 * 1.5)); /* G3 is the lowest: held factor, then the bass weight */
    assert(close_to(weights.pitch_class[4], 0.4));
}

static void test_every_member_on_the_bass_pitch_class_gets_the_bass_weight(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(48, true, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 3, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    /* Two C3s (one attacked, one held) at the bass, and a C4 above on the bass pitch class: all three take it. */
    assert(close_to(weights.pitch_class[0], 1.0 * 1.5 + 0.6 * 1.5 + 1.0 * 1.5));
}

static void test_the_bass_is_the_lowest_pitch_not_the_lowest_pitch_class(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[2] = {member(71, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 2, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.bass_pitch == 60 && weights.bass_pitch_class == 0); /* C4, though B (11) is a higher class */
}

static void test_a_bass_override_replaces_the_lowest_pitch(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[3] = {member(60, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(64, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(67, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;

    /* The override sounds as a member's pitch class (C): the bass weight multiplies the members on it. */
    int on_member = 36;
    assert(common_practice_weight_segment(members, 3, &config, &weights, &on_member) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.bass_pitch == 36 && weights.bass_pitch_class == 0);
    assert(close_to(weights.pitch_class[0], 1.5));
    assert(close_to(weights.pitch_class[4], 1.0) && close_to(weights.pitch_class[7], 1.0));

    /* The override does not sound (D): the weights are the plain ones and only the bass is set. */
    int silent = 38;
    assert(common_practice_weight_segment(members, 3, &config, &weights, &silent) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.bass_pitch == 38 && weights.bass_pitch_class == 2);
    assert(close_to(weights.pitch_class[0], 1.0) && close_to(weights.pitch_class[4], 1.0) &&
           close_to(weights.pitch_class[7], 1.0));
    assert(close_to(weights.total, 3.0));

    /* An override above the lowest member still replaces it: E4 is the bass, so C4 gets no bass weight. */
    int above = 64;
    assert(common_practice_weight_segment(members, 3, &config, &weights, &above) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.bass_pitch == 64 && weights.bass_pitch_class == 4);
    assert(close_to(weights.pitch_class[0], 1.0) && close_to(weights.pitch_class[4], 1.5));
}

static void test_no_override_is_byte_identical_to_the_lowest_pitch(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[4] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 0.5, BEAT_STRENGTH_MEDIUM),
                                       member(55, true, 1.0, BEAT_STRENGTH_STRONG),
                                       member(60, false, 0.25, BEAT_STRENGTH_WEAK)};
    CommonPracticeWeights without;
    CommonPracticeWeights lowest;
    int bass = 48; /* the lowest member */
    assert(common_practice_weight_segment(members, 4, &config, &without, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_weight_segment(members, 4, &config, &lowest, &bass) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(memcmp(&without, &lowest, sizeof(without)) == 0);
}

static void test_a_configured_factor_changes_the_weights(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    config.bass_weight = 2.0;
    config.held_factor = 1.0;
    CommonPracticeMember members[2] = {member(48, true, 1.0, BEAT_STRENGTH_WEAK),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, 2, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(weights.pitch_class[0], 2.0));
    assert(close_to(weights.pitch_class[4], 1.0));
}

static void test_the_result_is_deterministic(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember members[4] = {member(48, false, 0.3, BEAT_STRENGTH_STRONG),
                                       member(64, true, 0.7, BEAT_STRENGTH_WEAK),
                                       member(67, false, 0.1, BEAT_STRENGTH_MEDIUM),
                                       member(72, false, 0.45, BEAT_STRENGTH_WEAK)};
    CommonPracticeWeights a;
    CommonPracticeWeights b;
    assert(common_practice_weight_segment(members, 4, &config, &a, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_weight_segment(members, 4, &config, &b, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
}

static void test_no_members_weigh_nothing(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(NULL, 0, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(weights.total == 0.0 && weights.bass_pitch == -1 && weights.bass_pitch_class == -1);
}

static void test_rejects_bad_input_and_leaves_the_output_untouched(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeMember good = member(60, false, 1.0, BEAT_STRENGTH_STRONG);
    CommonPracticeWeights weights;
    memset(&weights, 0x7f, sizeof(weights));
    CommonPracticeWeights before = weights;

    assert(common_practice_weight_segment(&good, 1, NULL, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_weight_segment(&good, 1, &config, NULL, NULL) == COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_weight_segment(NULL, 2, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);

    CommonPracticeWeightedConfig negative = config;
    negative.held_factor = -0.1;
    assert(common_practice_weight_segment(&good, 1, &negative, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);
    CommonPracticeWeightedConfig not_finite = config;
    not_finite.bass_weight = INFINITY;
    assert(common_practice_weight_segment(&good, 1, &not_finite, &weights, NULL) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);

    CommonPracticeMember empty_duration = member(60, false, 0.0, BEAT_STRENGTH_STRONG);
    assert(common_practice_weight_segment(&empty_duration, 1, &config, &weights, NULL) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER);
    CommonPracticeMember bad_strength = member(60, false, 1.0, BEAT_STRENGTH_COUNT);
    assert(common_practice_weight_segment(&bad_strength, 1, &config, &weights, NULL) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_MEMBER);

    assert(memcmp(&weights, &before, sizeof(weights)) == 0);
}

/* --- step 2, scoring --- (key of C major unless stated: tonic pitch class 0) */

static CommonPracticeWeights weigh(const CommonPracticeMember *members, size_t count) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeWeights weights;
    assert(common_practice_weight_segment(members, count, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    return weights;
}

/* How many candidates the last rank() call returned (those whose root sounds). */
static size_t g_ranked_count;

static void rank(const CommonPracticeWeights *weights, int tonic, CommonPracticeRankedChord out[COMMON_PRACTICE_CANDIDATE_COUNT]) {
    CommonPracticeWeightedConfig config = bass_heavy();
    assert(common_practice_rank_candidates(weights, tonic, COMMON_PRACTICE_KEY_MAJOR, &config, out,
                                           &g_ranked_count) == COMMON_PRACTICE_WEIGHTED_OK);
}

static void assert_best(const CommonPracticeRankedChord *ranked, int degree, bool is_seventh) {
    assert(ranked[0].degree == degree && ranked[0].is_seventh == is_seventh);
}

static void test_a_pure_triad_is_covered_completely(void) {
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    rank(&weights, 0, ranked);
    assert_best(ranked, 1, false); /* I, and the triad beats the tied I major seventh */
    assert(ranked[0].triad == COMMON_PRACTICE_TRIAD_I);
    assert(close_to(ranked[0].score, 1.0 + 0.05)); /* all the weight, third present, bass is a chord tone */
}

static void test_the_candidates_are_the_fourteen_diatonic_chords_best_first(void) {
    /* All seven scale degrees sound, so every root is eligible: 14 candidates, one triad and one seventh
       on each degree. C D E F G A B, with C in the bass. */
    CommonPracticeMember members[7] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(62, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(64, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(65, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(67, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(69, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(71, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 7);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    rank(&weights, 0, ranked);
    assert(g_ranked_count == 14);

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
    CommonPracticeSeventh expected[8] = {0,
                                         COMMON_PRACTICE_SEVENTH_MAJOR7, COMMON_PRACTICE_SEVENTH_MINOR7,
                                         COMMON_PRACTICE_SEVENTH_MINOR7, COMMON_PRACTICE_SEVENTH_MAJOR7,
                                         COMMON_PRACTICE_SEVENTH_DOMINANT7, COMMON_PRACTICE_SEVENTH_MINOR7,
                                         COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7};
    for (int i = 0; i < 14; i++) {
        if (ranked[i].is_seventh) {
            assert(ranked[i].seventh == expected[ranked[i].degree]);
        }
    }
}

static void test_a_candidate_whose_root_does_not_sound_is_not_ranked(void) {
    /* C E G: only chords rooted on C, E or G are readings the notes support. A minor seventh (A C E G)
       covers all three notes but its root A is silent, so it is not ranked, and a bare triad does not tie
       with the seventh above it. */
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(52, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 3);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    rank(&weights, 0, ranked);
    assert(g_ranked_count == 6); /* I, iii, V, and the sevenths on them */
    for (size_t i = 0; i < g_ranked_count; i++) {
        assert(ranked[i].root_pitch_class == 0 || ranked[i].root_pitch_class == 4 || ranked[i].root_pitch_class == 7);
        assert(ranked[i].degree != 6);
    }
}

static void test_a_dominant_seventh_beats_the_dominant_triad(void) {
    CommonPracticeMember members[4] = {member(43, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(59, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(62, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(65, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 4);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
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
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
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
    CommonPracticeRankedChord full[COMMON_PRACTICE_CANDIDATE_COUNT];
    CommonPracticeRankedChord bare[COMMON_PRACTICE_CANDIDATE_COUNT];
    rank(&a, 0, full);
    rank(&b, 0, bare);
    assert_best(bare, 1, false); /* C and G alone are still best read as I... */
    assert(close_to(full[0].score - bare[0].score, 0.15)); /* ...but cost the 0.15 penalty */
}

/* The best chord of the default weights of `members` (quarters are 0.25, an eighth 0.125) under a seventh penalty. */
static CommonPracticeRankedChord best_with_seventh_penalty(const CommonPracticeMember *members, size_t count, double penalty) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.seventh_penalty = penalty;
    CommonPracticeWeights weights;
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t ranked_count = 0;
    assert(common_practice_weight_segment(members, count, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, &ranked_count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(ranked_count > 0);
    return ranked[0];
}

static void test_a_seventh_penalty_keeps_a_passing_tone_from_completing_a_seventh(void) {
    /* C3 E4 G4 quarters on a strong beat and a passing A4 eighth on a weak beat. */
    CommonPracticeMember members[4] = {member(48, false, 0.25, BEAT_STRENGTH_STRONG), member(64, false, 0.25, BEAT_STRENGTH_STRONG),
                                       member(67, false, 0.25, BEAT_STRENGTH_STRONG), member(69, false, 0.125, BEAT_STRENGTH_WEAK)};
    CommonPracticeRankedChord free_reading = best_with_seventh_penalty(members, 4, 0.0);
    assert(free_reading.is_seventh && free_reading.root_pitch_class == 9); /* A minor seventh: A C E G */
    CommonPracticeRankedChord charged = best_with_seventh_penalty(members, 4, 0.15);
    assert(!charged.is_seventh && charged.root_pitch_class == 0); /* C major */
}

static void test_a_seventh_with_real_weight_survives_the_penalty(void) {
    /* G3 B3 D4 F4 quarters: the F carries a full share, so the dominant seventh still outranks its triad. */
    CommonPracticeMember members[4] = {member(55, false, 0.25, BEAT_STRENGTH_STRONG), member(59, false, 0.25, BEAT_STRENGTH_STRONG),
                                       member(62, false, 0.25, BEAT_STRENGTH_STRONG), member(65, false, 0.25, BEAT_STRENGTH_STRONG)};
    CommonPracticeRankedChord charged = best_with_seventh_penalty(members, 4, 0.15);
    assert(charged.is_seventh && charged.seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7 && charged.root_pitch_class == 7);
}

static void test_a_zero_seventh_penalty_changes_nothing_and_a_bad_one_is_rejected(void) {
    CommonPracticeMember members[4] = {member(48, false, 0.25, BEAT_STRENGTH_STRONG), member(64, false, 0.25, BEAT_STRENGTH_STRONG),
                                       member(67, false, 0.25, BEAT_STRENGTH_STRONG), member(69, false, 0.125, BEAT_STRENGTH_WEAK)};
    CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();
    CommonPracticeWeightedConfig zero = defaults;
    zero.seventh_penalty = 0.0;
    CommonPracticeWeights weights;
    CommonPracticeRankedChord a[COMMON_PRACTICE_CANDIDATE_COUNT];
    CommonPracticeRankedChord b[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count_a = 0;
    size_t count_b = 0;
    assert(common_practice_weight_segment(members, 4, &defaults, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &defaults, a, &count_a) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &zero, b, &count_b) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(count_a == count_b && memcmp(a, b, count_a * sizeof(a[0])) == 0);

    CommonPracticeWeightedConfig negative = defaults;
    negative.seventh_penalty = -0.1;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &negative, a, &count_a) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);
    CommonPracticeWeightedConfig not_finite = defaults;
    not_finite.seventh_penalty = INFINITY;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &not_finite, a, &count_a) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);
}

/* The position of a diatonic triad (is_seventh false) of the given degree in a ranking. */
static int position_of_triad(const CommonPracticeRankedChord *ranked, int degree) {
    for (int i = 0; i < (int)g_ranked_count; i++) {
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
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
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
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
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
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
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
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    rank(&weights, 0, ranked);
    assert_best(ranked, 1, false);
}

static void test_requiring_the_bass_chord_tone_limits_the_candidates(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeMember members[3] = {member(60, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(64, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(67, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights;
    CommonPracticeRankedChord all[COMMON_PRACTICE_CANDIDATE_COUNT];
    CommonPracticeRankedChord held[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t all_count = 0;
    size_t held_count = 0;

    /* E in the bass: only chords with an E remain (I, I7, iii, iii7); V and V7 are out. */
    int e_bass = 40;
    assert(common_practice_weight_segment(members, 3, &config, &weights, &e_bass) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, all, &all_count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    config.require_bass_chord_tone = true;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, held, &held_count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(held_count > 0 && held_count < all_count);
    for (size_t i = 0; i < held_count; i++) {
        assert((common_practice_ranked_chord_mask(&held[i], 0) & (1u << 4)) != 0);
    }

    /* F# in the bass: no C major chord holds it, so every candidate stays eligible, in the same order. */
    int fsharp_bass = 42;
    assert(common_practice_weight_segment(members, 3, &config, &weights, &fsharp_bass) == COMMON_PRACTICE_WEIGHTED_OK);
    config.require_bass_chord_tone = false;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, all, &all_count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    config.require_bass_chord_tone = true;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, held, &held_count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(held_count == all_count);
    for (size_t i = 0; i < all_count; i++) {
        assert(all[i].degree == held[i].degree && all[i].root_pitch_class == held[i].root_pitch_class &&
               all[i].is_seventh == held[i].is_seventh && all[i].triad == held[i].triad &&
               all[i].seventh == held[i].seventh && all[i].score == held[i].score);
    }

    /* Without a bass (no members), the flag changes nothing. */
    assert(common_practice_weight_segment(NULL, 0, &config, &weights, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, held, &held_count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(held_count == 0);
}

static void test_an_empty_segment_ranks_without_a_bass(void) {
    CommonPracticeWeights weights = weigh(NULL, 0);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    rank(&weights, 0, ranked);
    assert(g_ranked_count == 0); /* nothing sounds, so no root sounds: nothing to rank */
}

static void test_scoring_rejects_bad_input(void) {
    CommonPracticeWeightedConfig config = bass_heavy();
    CommonPracticeWeights weights = weigh(NULL, 0);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = 0;
    assert(common_practice_rank_candidates(NULL, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, NULL) ==
           COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MINOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_OK); /* minor has its own vocabulary */
    assert(common_practice_rank_candidates(&weights, 0, (CommonPracticeKeyMode)99, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE);
    assert(common_practice_rank_candidates(&weights, 12, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC);
    assert(common_practice_rank_candidates(&weights, -1, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC);
    config.bass_weight = -1.0;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_CONFIG);
}

/* --- applied chords (R-380) --- */

static CommonPracticeWeights weigh_all_pitch_classes(void) {
    CommonPracticeMember members[12];
    for (int i = 0; i < 12; i++) {
        members[i] = member(48 + i, false, 1.0, BEAT_STRENGTH_STRONG);
    }
    return weigh(members, 12);
}

static size_t rank_applied(const CommonPracticeWeights *weights, int tonic, CommonPracticeKeyMode mode, bool applied,
                           bool require_bass, CommonPracticeRankedChord out[COMMON_PRACTICE_CANDIDATE_COUNT]) {
    CommonPracticeWeightedConfig config = bass_heavy();
    config.applied_chords = applied;
    config.require_bass_chord_tone = require_bass;
    size_t count = 0;
    assert(common_practice_rank_candidates(weights, tonic, mode, &config, out, &count) == COMMON_PRACTICE_WEIGHTED_OK);
    return count;
}

static const CommonPracticeRankedChord *find_applied(const CommonPracticeRankedChord *ranked, size_t count, int root,
                                                     bool is_seventh, int target) {
    for (size_t i = 0; i < count; i++) {
        if (ranked[i].applied_target_degree == target && ranked[i].is_seventh == is_seventh &&
            ranked[i].root_pitch_class == root) {
            return &ranked[i];
        }
    }
    return NULL;
}

static void test_the_applied_vocabulary_has_seventeen_and_ten_chords(void) {
    CommonPracticeWeights weights = weigh_all_pitch_classes();
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    assert(COMMON_PRACTICE_MAJOR_APPLIED_COUNT == 17 && COMMON_PRACTICE_MINOR_APPLIED_COUNT == 10);
    assert(COMMON_PRACTICE_CANDIDATE_COUNT == 36);

    size_t off = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, false, false, ranked);
    assert(off == 14);
    for (size_t i = 0; i < off; i++) {
        assert(ranked[i].applied_target_degree == 0);
    }
    size_t major = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, true, false, ranked);
    assert(major == 14 + 17);
    size_t applied = 0;
    for (size_t i = 0; i < major; i++) {
        applied += ranked[i].applied_target_degree != 0;
    }
    assert(applied == 17);

    off = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MINOR, false, false, ranked);
    assert(off == 26);
    size_t minor = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MINOR, true, false, ranked);
    assert(minor == 26 + 10);
    applied = 0;
    for (size_t i = 0; i < minor; i++) {
        applied += ranked[i].applied_target_degree != 0;
    }
    assert(applied == 10);
}

static void test_applied_chords_carry_the_function_spelling(void) {
    CommonPracticeWeights weights = weigh_all_pitch_classes();
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, true, false, ranked);

    const CommonPracticeRankedChord *v_of_v = find_applied(ranked, count, 2, false, 5); /* D F# A */
    assert(v_of_v != NULL && v_of_v->degree == 2 && v_of_v->triad == COMMON_PRACTICE_TRIAD_MAJOR);
    assert(v_of_v->tonic_offset == 2 && v_of_v->form_rank == 0);
    const CommonPracticeRankedChord *v7_of_v = find_applied(ranked, count, 2, true, 5);
    assert(v7_of_v != NULL && v7_of_v->seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7);
    const CommonPracticeRankedChord *v_of_ii = find_applied(ranked, count, 9, false, 2); /* A C# E */
    assert(v_of_ii != NULL && v_of_ii->degree == 6);
    const CommonPracticeRankedChord *vii_of_v = find_applied(ranked, count, 6, false, 5); /* F# A C */
    assert(vii_of_v != NULL && vii_of_v->degree == 4 && vii_of_v->triad == COMMON_PRACTICE_TRIAD_DIMINISHED);
    const CommonPracticeRankedChord *vii7_of_v = find_applied(ranked, count, 6, true, 5); /* F# A C Eb */
    assert(vii7_of_v != NULL && vii7_of_v->degree == 4 && vii7_of_v->seventh == COMMON_PRACTICE_SEVENTH_DIMINISHED7);
    assert(find_applied(ranked, count, 3, true, 3) == NULL); /* vii-dim-7/iii shares the set of vii-dim-7/V: one entry */
    assert(find_applied(ranked, count, 4, true, 4) == NULL); /* so does vii-dim-7/IV with /ii */
    assert(find_applied(ranked, count, 1, true, 2) != NULL);
    assert(find_applied(ranked, count, 7, false, 5) == NULL); /* V/IV is I: diatonic, left out */

    count = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MINOR, true, false, ranked);
    assert(find_applied(ranked, count, 0, true, 4) != NULL);  /* V7/iv C7 */
    assert(find_applied(ranked, count, 3, true, 6) != NULL);  /* V7/VI Eb7 */
    assert(find_applied(ranked, count, 2, false, 5) != NULL); /* V/V D */
    assert(find_applied(ranked, count, 10, true, 3) == NULL); /* V7/III is the diatonic VII7 */
    assert(find_applied(ranked, count, 2, true, 3) == NULL);  /* vii-dim-7/III is the diatonic vii-dim-7 */
    assert(find_applied(ranked, count, 9, true, 7) == NULL);  /* vii-dim-7/VII shares the set of /V */
}

static void test_an_applied_chord_needs_a_tone_outside_the_key(void) {
    /* D3 A3 alone: V/V (D F# A) has no F# sounding, so it is not eligible; ii is the best. */
    CommonPracticeMember members[2] = {member(50, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(57, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 2);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, true, false, ranked);
    assert(count > 0 && ranked[0].degree == 2 && ranked[0].applied_target_degree == 0);
    for (size_t i = 0; i < count; i++) {
        assert(ranked[i].applied_target_degree == 0);
    }
    assert(count == rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, false, false, ranked));
}

static void test_an_applied_chord_must_hold_a_required_bass(void) {
    /* D4 F#4 A4 over an F3 bass: with the bass required, no applied chord holds F (V/V is D F# A), so none is ranked. */
    CommonPracticeMember members[4] = {member(53, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(62, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(66, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(69, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 4);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t free_count = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, true, false, ranked);
    assert(find_applied(ranked, free_count, 2, false, 5) != NULL);
    size_t held = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, true, true, ranked);
    assert(held > 0 && held < free_count);
    for (size_t i = 0; i < held; i++) {
        assert(ranked[i].applied_target_degree == 0);
        assert((common_practice_ranked_chord_mask(&ranked[i], 0) & (1u << 5)) != 0);
    }
}

static void test_a_diatonic_chord_ranks_ahead_of_an_applied_one_on_a_tie(void) {
    /* C E G B Bb in equal weights, no bass weight: I major 7 (C E G B) and V7/IV (C E G Bb) cover the same weight. */
    CommonPracticeMember members[5] = {member(60, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(64, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(67, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(71, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(70, false, 1.0, BEAT_STRENGTH_STRONG)};
    CommonPracticeWeights weights = weigh(members, 5);
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, true, false, ranked);
    size_t diatonic = count;
    size_t applied = count;
    for (size_t i = 0; i < count; i++) {
        if (ranked[i].root_pitch_class == 0 && ranked[i].is_seventh) {
            if (ranked[i].applied_target_degree == 0 && diatonic == count) {
                diatonic = i;
            }
            if (ranked[i].applied_target_degree == 4) {
                applied = i;
            }
        }
    }
    assert(diatonic < count && applied < count);
    assert(fabs(ranked[diatonic].score - ranked[applied].score) < 1e-9);
    assert(diatonic < applied);
}

static void test_the_ranking_without_the_flag_is_unchanged(void) {
    CommonPracticeWeights weights = weigh_all_pitch_classes();
    CommonPracticeRankedChord with_default[COMMON_PRACTICE_CANDIDATE_COUNT];
    CommonPracticeRankedChord flag_off[COMMON_PRACTICE_CANDIDATE_COUNT];
    CommonPracticeWeightedConfig config = bass_heavy();
    size_t a = 0;
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MINOR, &config, with_default, &a) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    size_t b = rank_applied(&weights, 0, COMMON_PRACTICE_KEY_MINOR, false, false, flag_off);
    assert(a == b && a == 26);
    for (size_t i = 0; i < a; i++) {
        assert(with_default[i].degree == flag_off[i].degree && with_default[i].root_pitch_class == flag_off[i].root_pitch_class &&
               with_default[i].score == flag_off[i].score && with_default[i].applied_target_degree == 0);
    }
}

/* A resolved accent (R-382) carries no weight, nor does it count in the total, and it is no bass; unflagged members weigh as before. */
static void test_a_resolved_accent_carries_no_weight(void) {
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG), member(67, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(77, false, 0.25, BEAT_STRENGTH_STRONG)}; /* C3, G4 and an F5 accent */
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeWeights with_accent;
    assert(common_practice_weight_segment(members, 3, &config, &with_accent, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(close_to(with_accent.pitch_class[5], 0.25) && close_to(with_accent.total, 1.1 + 1.0 + 0.25));

    members[2].resolves_in_segment = true;
    CommonPracticeWeights flagged;
    assert(common_practice_weight_segment(members, 3, &config, &flagged, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(flagged.pitch_class[5] == 0.0 && close_to(flagged.total, 1.1 + 1.0));
    assert(close_to(flagged.pitch_class[0], with_accent.pitch_class[0]) && close_to(flagged.pitch_class[7], with_accent.pitch_class[7]));
    assert(flagged.bass_pitch == 48 && flagged.bass_pitch_class == 0);

    /* the lowest unflagged member is the bass: a flagged lowest note is none */
    CommonPracticeMember low_accent[2] = {member(36, false, 0.25, BEAT_STRENGTH_STRONG), member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    low_accent[0].resolves_in_segment = true;
    CommonPracticeWeights over_g;
    assert(common_practice_weight_segment(low_accent, 2, &config, &over_g, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(over_g.bass_pitch == 55 && over_g.pitch_class[0] == 0.0 && close_to(over_g.total, 1.1));

    /* nothing flagged (the zero value): as before. A member built from a note is never flagged. */
    CommonPracticeMember plain = member(60, false, 1.0, BEAT_STRENGTH_STRONG);
    assert(!plain.resolves_in_segment);
}

/* With weigh_resolved_accents (the key evidence, R-386) a flagged member has the weight it has unflagged and can be the bass; false, the
   default, skips it as before. */
static void test_a_resolved_accent_is_key_evidence_when_asked(void) {
    CommonPracticeMember members[3] = {member(48, false, 1.0, BEAT_STRENGTH_STRONG), member(67, false, 1.0, BEAT_STRENGTH_STRONG),
                                       member(77, false, 0.25, BEAT_STRENGTH_STRONG)}; /* C3, G4 and an F5 accent */
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    assert(!config.weigh_resolved_accents);
    CommonPracticeWeights unflagged;
    assert(common_practice_weight_segment(members, 3, &config, &unflagged, NULL) == COMMON_PRACTICE_WEIGHTED_OK);

    members[2].resolves_in_segment = true;
    CommonPracticeWeights skipped;
    assert(common_practice_weight_segment(members, 3, &config, &skipped, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(skipped.pitch_class[5] == 0.0 && skipped.total < unflagged.total);

    config.weigh_resolved_accents = true;
    CommonPracticeWeights counted;
    assert(common_practice_weight_segment(members, 3, &config, &counted, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    for (int pc = 0; pc < 12; pc++) {
        assert(counted.pitch_class[pc] == unflagged.pitch_class[pc]);
    }
    assert(counted.total == unflagged.total && counted.bass_pitch == unflagged.bass_pitch);

    /* it can be the bass: a flagged lowest note is the bass when counted, none when skipped */
    CommonPracticeMember low[2] = {member(36, false, 0.25, BEAT_STRENGTH_STRONG), member(55, false, 1.0, BEAT_STRENGTH_STRONG)};
    low[0].resolves_in_segment = true;
    CommonPracticeWeights as_bass;
    assert(common_practice_weight_segment(low, 2, &config, &as_bass, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(as_bass.bass_pitch == 36 && as_bass.bass_pitch_class == 0);
    config.weigh_resolved_accents = false;
    assert(common_practice_weight_segment(low, 2, &config, &as_bass, NULL) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(as_bass.bass_pitch == 55);
}

int main(void) {
    test_default_config();
    test_a_pure_triad_weighs_its_three_pitch_classes();
    test_duration_and_beat_strength_scale_a_member();
    test_a_held_member_uses_the_held_factor_not_its_attack_strength();
    test_every_member_on_the_bass_pitch_class_gets_the_bass_weight();
    test_the_bass_is_the_lowest_pitch_not_the_lowest_pitch_class();
    test_a_bass_override_replaces_the_lowest_pitch();
    test_no_override_is_byte_identical_to_the_lowest_pitch();
    test_a_configured_factor_changes_the_weights();
    test_the_result_is_deterministic();
    test_no_members_weigh_nothing();
    test_rejects_bad_input_and_leaves_the_output_untouched();
    test_a_pure_triad_is_covered_completely();
    test_the_candidates_are_the_fourteen_diatonic_chords_best_first();
    test_a_candidate_whose_root_does_not_sound_is_not_ranked();
    test_a_dominant_seventh_beats_the_dominant_triad();
    test_the_key_is_transposed();
    test_a_missing_third_is_penalized();
    test_a_seventh_penalty_keeps_a_passing_tone_from_completing_a_seventh();
    test_a_seventh_with_real_weight_survives_the_penalty();
    test_a_zero_seventh_penalty_changes_nothing_and_a_bad_one_is_rejected();
    test_the_bass_bonus_decides_between_i_and_iii();
    test_a_tie_goes_to_root_position();
    test_a_tie_goes_to_the_triad_over_the_seventh();
    test_a_second_inversion_does_not_hide_the_chord();
    test_an_empty_segment_ranks_without_a_bass();
    test_requiring_the_bass_chord_tone_limits_the_candidates();
    test_scoring_rejects_bad_input();
    test_the_applied_vocabulary_has_seventeen_and_ten_chords();
    test_applied_chords_carry_the_function_spelling();
    test_an_applied_chord_needs_a_tone_outside_the_key();
    test_an_applied_chord_must_hold_a_required_bass();
    test_a_diatonic_chord_ranks_ahead_of_an_applied_one_on_a_tie();
    test_the_ranking_without_the_flag_is_unchanged();
    test_a_resolved_accent_carries_no_weight();
    test_a_resolved_accent_is_key_evidence_when_asked();
    printf("test_weighted_identification: ok\n");
    return 0;
}
