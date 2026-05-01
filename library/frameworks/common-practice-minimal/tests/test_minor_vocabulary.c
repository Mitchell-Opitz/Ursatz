#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "weighted_chord_identification.h"

/*
 * Minor-key vocabulary (R-030): the diatonic triads and sevenths of natural, harmonic and melodic minor,
 * including the augmented III and the fully-diminished vii-dim-7, without the minor-major and
 * augmented-major sevenths; the raised form wins an exact tie for V, V7, vii-dim and vii-dim-7 and the
 * natural form everywhere else. Every test is in A minor (tonic pitch class 9).
 */

#define A_MINOR_TONIC 9

#define A 57
#define B 59
#define C 60
#define D 62
#define E 64
#define F 65
#define FS 66
#define G 67
#define GS 68

static CommonPracticeMember note(int pitch) {
    CommonPracticeMember m;
    memset(&m, 0, sizeof(m));
    m.pitch = pitch;
    m.duration = 1.0;
    m.strength = BEAT_STRENGTH_STRONG;
    return m;
}

/* The best chord of one segment of equally weighted notes (the first is the lowest), read in A minor. */
static CommonPracticeWeightedResult identify(const int *pitches, size_t count) {
    CommonPracticeMember members[8];
    for (size_t i = 0; i < count; i++) {
        members[i] = note(pitches[i]);
    }
    CommonPracticeSegment segment = {members, count, "sonority-minor"};
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&segment, A_MINOR_TONIC, COMMON_PRACTICE_KEY_MINOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched);
    return result;
}

static void expect_intervals(const CommonPracticeWeightedResult *result, const int *want, size_t count) {
    ChordStructure structure;
    assert(chord_structure(result->best_chord, &structure));
    assert(structure.interval_count == count);
    for (size_t i = 0; i < count; i++) {
        assert(structure.root_relative_intervals[i] == want[i]);
    }
}

static void test_harmonic_minor_dominant_seventh(void) {
    const int pitches[4] = {E - 12, GS, B, D + 12}; /* E G# B D: V7 */
    CommonPracticeWeightedResult result = identify(pitches, 4);
    assert(result.best.degree == 5 && result.best.is_seventh && result.best.seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7);
    assert(result.best.root_pitch_class == 4);
    assert(strcmp(chord_label(result.best_chord), "dominant7") == 0);
    const int want[4] = {0, 4, 7, 10};
    expect_intervals(&result, want, 4);
    common_practice_weighted_result_destroy(&result);
}

static void test_harmonic_minor_fully_diminished_seventh(void) {
    const int pitches[4] = {GS - 12, B, D + 12, F + 12}; /* G# B D F: vii-dim-7 */
    CommonPracticeWeightedResult result = identify(pitches, 4);
    assert(result.best.degree == 7 && result.best.is_seventh && result.best.seventh == COMMON_PRACTICE_SEVENTH_DIMINISHED7);
    assert(result.best.root_pitch_class == 8);
    assert(strcmp(chord_label(result.best_chord), "diminished7") == 0);
    const int want[4] = {0, 3, 6, 9};
    expect_intervals(&result, want, 4);
    common_practice_weighted_result_destroy(&result);
}

static void test_the_augmented_third_degree_triad(void) {
    const int pitches[3] = {C, E, GS}; /* C E G#: III+ */
    CommonPracticeWeightedResult result = identify(pitches, 3);
    assert(result.best.degree == 3 && !result.best.is_seventh && result.best.triad == COMMON_PRACTICE_TRIAD_AUGMENTED);
    assert(strcmp(chord_label(result.best_chord), "augmented") == 0);
    const int want[3] = {0, 4, 8};
    expect_intervals(&result, want, 3);
    common_practice_weighted_result_destroy(&result);
}

static void test_melodic_minor_major_subdominant_and_natural_minor_subdominant(void) {
    const int melodic[3] = {D, FS, A + 12}; /* D F# A: the melodic minor IV */
    CommonPracticeWeightedResult result = identify(melodic, 3);
    assert(result.best.degree == 4 && !result.best.is_seventh && result.best.triad == COMMON_PRACTICE_TRIAD_MAJOR);
    assert(result.best.root_pitch_class == 2);
    common_practice_weighted_result_destroy(&result);

    const int natural[3] = {D, F, A + 12}; /* D F A: iv */
    result = identify(natural, 3);
    assert(result.best.degree == 4 && result.best.triad == COMMON_PRACTICE_TRIAD_MINOR);
    common_practice_weighted_result_destroy(&result);
}

static void test_natural_minor_v_and_the_raised_dominant(void) {
    const int natural[3] = {E - 12, G, B}; /* E G B: v */
    CommonPracticeWeightedResult result = identify(natural, 3);
    assert(result.best.degree == 5 && result.best.triad == COMMON_PRACTICE_TRIAD_MINOR);
    assert(result.best.root_pitch_class == 4);
    common_practice_weighted_result_destroy(&result);

    const int raised[3] = {E - 12, GS, B}; /* E G# B: V */
    result = identify(raised, 3);
    assert(result.best.degree == 5 && result.best.triad == COMMON_PRACTICE_TRIAD_MAJOR);
    common_practice_weighted_result_destroy(&result);
}

static void test_the_subtonic_and_the_natural_sevenths(void) {
    const int vii[4] = {G - 12, B, D + 12, F + 12}; /* G B D F: VII7, the natural minor subtonic seventh */
    CommonPracticeWeightedResult result = identify(vii, 4);
    assert(result.best.degree == 7 && result.best.is_seventh && result.best.seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7);
    assert(result.best.root_pitch_class == 7);
    common_practice_weighted_result_destroy(&result);

    const int ii[4] = {B - 12, D, F, A}; /* B D F A: ii half-diminished seventh */
    result = identify(ii, 4);
    assert(result.best.degree == 2 && result.best.is_seventh &&
           result.best.seventh == COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7);
    common_practice_weighted_result_destroy(&result);
}

/* --- the tie rule: the same score, the raised form for V, V7, vii-dim and vii-dim-7, the natural form elsewhere --- */

static void test_a_tie_goes_to_the_raised_dominant(void) {
    const int pitches[2] = {E - 12, B}; /* E B: no third, so V and v score the same */
    CommonPracticeWeightedResult result = identify(pitches, 2);
    assert(result.best.root_pitch_class == 4 && !result.best.is_seventh);
    assert(result.best.triad == COMMON_PRACTICE_TRIAD_MAJOR);
    common_practice_weighted_result_destroy(&result);
}

static void test_a_tie_goes_to_the_natural_form_elsewhere(void) {
    const int pitches[2] = {D, A + 12}; /* D A: iv and IV score the same */
    CommonPracticeWeightedResult result = identify(pitches, 2);
    assert(result.best.root_pitch_class == 2 && !result.best.is_seventh);
    assert(result.best.triad == COMMON_PRACTICE_TRIAD_MINOR);
    common_practice_weighted_result_destroy(&result);
}

/* --- ranking --- */

static void test_all_twenty_six_minor_candidates_rank_when_every_root_sounds(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    CommonPracticeWeights weights;
    memset(&weights, 0, sizeof(weights));
    for (int pc = 0; pc < 12; pc++) {
        weights.pitch_class[pc] = 1.0;
        weights.total += 1.0;
    }
    weights.bass_pitch = 57;
    weights.bass_pitch_class = 9;
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t count = 0;
    assert(common_practice_rank_candidates(&weights, A_MINOR_TONIC, COMMON_PRACTICE_KEY_MINOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(count == COMMON_PRACTICE_MINOR_CANDIDATE_COUNT && count == 26);
    for (size_t i = 0; i < count; i++) {
        int intervals[4];
        size_t interval_count = common_practice_ranked_chord_intervals(&ranked[i], intervals);
        assert(interval_count == (ranked[i].is_seventh ? 4u : 3u));
        /* no minor-major seventh {0,3,7,11} and no augmented-major seventh {0,4,8,11} */
        assert(!(interval_count == 4 && intervals[3] == 11 && (intervals[1] == 3 || intervals[2] == 8)));
        if (i > 0) {
            assert(ranked[i - 1].score >= ranked[i].score - 1e-9);
        }
    }
    /* the major key still has fourteen */
    assert(common_practice_rank_candidates(&weights, 0, COMMON_PRACTICE_KEY_MAJOR, &config, ranked, &count) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(count == COMMON_PRACTICE_MAJOR_CANDIDATE_COUNT);
}

static void test_the_mask_follows_the_chords_own_quality(void) {
    CommonPracticeRankedChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = 3;
    chord.triad = COMMON_PRACTICE_TRIAD_AUGMENTED;
    chord.root_pitch_class = 0;
    assert(common_practice_ranked_chord_mask(&chord, A_MINOR_TONIC) == ((1u << 0) | (1u << 4) | (1u << 8)));
    chord.degree = 7;
    chord.is_seventh = true;
    chord.seventh = COMMON_PRACTICE_SEVENTH_DIMINISHED7;
    chord.root_pitch_class = 8;
    assert(common_practice_ranked_chord_mask(&chord, A_MINOR_TONIC) == ((1u << 8) | (1u << 11) | (1u << 2) | (1u << 5)));
}

static void test_a_sequence_reads_in_minor(void) {
    CommonPracticeMember a_minor[3] = {note(A - 12), note(C), note(E)};
    CommonPracticeMember e_seven[4] = {note(E - 12), note(GS), note(B), note(D + 12)};
    CommonPracticeSegment segments[2] = {{a_minor, 3, "s1"}, {e_seven, 4, "s2"}};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, A_MINOR_TONIC, COMMON_PRACTICE_KEY_MINOR, NULL,
                                                      &results) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.count == 2 && results.items[0].matched && results.items[1].matched);
    assert(results.items[0].best.degree == 1 && results.items[0].best.triad == COMMON_PRACTICE_TRIAD_MINOR);
    assert(results.items[1].best.degree == 5 && results.items[1].best.is_seventh);
    common_practice_weighted_results_destroy(&results);
}

int main(void) {
    test_harmonic_minor_dominant_seventh();
    test_harmonic_minor_fully_diminished_seventh();
    test_the_augmented_third_degree_triad();
    test_melodic_minor_major_subdominant_and_natural_minor_subdominant();
    test_natural_minor_v_and_the_raised_dominant();
    test_the_subtonic_and_the_natural_sevenths();
    test_a_tie_goes_to_the_raised_dominant();
    test_a_tie_goes_to_the_natural_form_elsewhere();
    test_all_twenty_six_minor_candidates_rank_when_every_root_sounds();
    test_the_mask_follows_the_chords_own_quality();
    test_a_sequence_reads_in_minor();
    printf("test_minor_vocabulary: ok\n");
    return 0;
}
