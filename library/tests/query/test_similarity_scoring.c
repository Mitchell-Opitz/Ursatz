#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "query/similarity_scoring.h"

/*
 * Each dimension's exact definition (query/similarity_scoring.h), checked on
 * hand-worked sequences: identical sequences score 1, sequences that disagree
 * at every position score 0, a dimension that is not declared contributes
 * nothing, intervals are transposition-invariant, contour compares directions,
 * and a uniformly augmented or diminished rhythm still matches.
 */

static void assert_score(SimilarityScore score, unsigned long numerator, unsigned long denominator) {
    assert(score.numerator == numerator && score.denominator == denominator);
}

static SimilarityModel *make_model(const SimilarityDimensionWeight *weights, size_t count) {
    SimilarityModel *model = NULL;
    assert(similarity_model_create(weights, count, &model) == SIMILARITY_MODEL_OK);
    return model;
}

static void test_interval_dimension(void) {
    const long melody[] = {2, 2, -3, 0};
    SimilarityScore score = {9, 9};

    /* Identical: 1. A transposed copy has the same intervals, so it is the same sequence: 1. */
    assert(similarity_interval_score(melody, 4, melody, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);
    const long transposed_copy[] = {2, 2, -3, 0};
    assert(similarity_interval_score(melody, 4, transposed_copy, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);

    /* Two of four positions agree: 1/2. One of three: 1/3. */
    const long half[] = {2, 1, -3, 5};
    assert(similarity_interval_score(melody, 4, half, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 2);
    const long three[] = {2, 7, 7};
    const long three_other[] = {2, 2, 2};
    assert(similarity_interval_score(three, 3, three_other, 3, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 3);

    /* Different at every position: 0. A different length: 0. */
    const long different[] = {9, 9, 9, 9};
    assert(similarity_interval_score(melody, 4, different, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);
    assert(similarity_interval_score(melody, 4, melody, 3, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);

    /* An empty sequence cannot be compared; missing arguments are reported. The output is untouched on error. */
    score.numerator = 5;
    score.denominator = 5;
    assert(similarity_interval_score(melody, 0, melody, 4, &score) == SIMILARITY_SCORE_ERR_INVALID_INPUT);
    assert(similarity_interval_score(NULL, 0, melody, 4, &score) == SIMILARITY_SCORE_ERR_INVALID_INPUT);
    assert(similarity_interval_score(NULL, 4, melody, 4, &score) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
    assert(similarity_interval_score(melody, 4, melody, 4, NULL) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
    assert_score(score, 5, 5);
}

static void test_contour_dimension(void) {
    const ContourDirection melody[] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN,
                                       CONTOUR_DIRECTION_SAME};
    const ContourDirection three_quarters[] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_DOWN,
                                               CONTOUR_DIRECTION_SAME};
    const ContourDirection opposite[] = {CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_UP,
                                         CONTOUR_DIRECTION_UP};
    SimilarityScore score;
    assert(similarity_contour_score(melody, 4, melody, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);
    assert(similarity_contour_score(melody, 4, three_quarters, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 3, 4);
    /* Disagrees at every position, including SAME against UP. */
    const ContourDirection all_different[] = {CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_DOWN, CONTOUR_DIRECTION_UP,
                                              CONTOUR_DIRECTION_UP};
    assert(similarity_contour_score(melody, 4, all_different, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);
    assert(similarity_contour_score(melody, 4, opposite, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);
    assert(similarity_contour_score(melody, 4, melody, 2, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);
    assert(similarity_contour_score(melody, 0, melody, 4, &score) == SIMILARITY_SCORE_ERR_INVALID_INPUT);

    /* Directions derived from intervals: positive up, negative down, zero same. */
    const long intervals[] = {2, 2, -3, 0, 7, -1};
    ContourDirection derived[6];
    similarity_directions_from_intervals(intervals, 6, derived);
    assert(derived[0] == CONTOUR_DIRECTION_UP && derived[1] == CONTOUR_DIRECTION_UP && derived[2] == CONTOUR_DIRECTION_DOWN &&
           derived[3] == CONTOUR_DIRECTION_SAME && derived[4] == CONTOUR_DIRECTION_UP &&
           derived[5] == CONTOUR_DIRECTION_DOWN);
}

static void test_rhythm_dimension(void) {
    /* Durations in whole notes: quarter, quarter, half, quarter. Consecutive ratios: 1, 2, 1/2. */
    const SimilarityDuration pattern[] = {{1, 4}, {1, 4}, {1, 2}, {1, 4}};
    SimilarityScore score;
    assert(similarity_rhythm_score(pattern, 4, pattern, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);

    /* Uniformly augmented (every duration doubled) and diminished (halved): the same ratios, so 1. */
    const SimilarityDuration augmented[] = {{1, 2}, {1, 2}, {1, 1}, {1, 2}};
    const SimilarityDuration diminished[] = {{1, 8}, {1, 8}, {1, 4}, {1, 8}};
    assert(similarity_rhythm_score(pattern, 4, augmented, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);
    assert(similarity_rhythm_score(pattern, 4, diminished, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);
    /* Scaled by a non-integer (times 3/2), and written unreduced (2/8 for 1/4): still 1. */
    const SimilarityDuration scaled[] = {{3, 8}, {3, 8}, {3, 4}, {3, 8}};
    const SimilarityDuration unreduced[] = {{2, 8}, {3, 12}, {4, 8}, {5, 20}};
    assert(similarity_rhythm_score(pattern, 4, scaled, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);
    assert(similarity_rhythm_score(pattern, 4, unreduced, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);

    /* A different rhythm: ratios 2, 1, 1/2 against 1, 2, 1/2: only the last of three agrees. */
    const SimilarityDuration other[] = {{1, 4}, {1, 2}, {1, 2}, {1, 4}};
    assert(similarity_rhythm_score(pattern, 4, other, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 3);
    /* Every ratio different: 0. */
    const SimilarityDuration all_different[] = {{1, 4}, {1, 2}, {1, 4}, {1, 2}}; /* ratios 2, 1/2, 2 */
    assert(similarity_rhythm_score(pattern, 4, all_different, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);

    /* A single duration has no ratios and matches another single duration; different counts score 0. */
    const SimilarityDuration one[] = {{1, 4}};
    const SimilarityDuration another_one[] = {{3, 8}};
    assert(similarity_rhythm_score(one, 1, another_one, 1, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 1, 1);
    assert(similarity_rhythm_score(one, 1, pattern, 4, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);
    assert(similarity_rhythm_score(pattern, 4, pattern, 3, &score) == SIMILARITY_SCORE_OK);
    assert_score(score, 0, 1);

    /* A duration with a zero part, or an empty pattern, cannot be compared. */
    const SimilarityDuration zero_numerator[] = {{0, 4}, {1, 4}};
    const SimilarityDuration zero_denominator[] = {{1, 0}, {1, 4}};
    const SimilarityDuration pair[] = {{1, 4}, {1, 4}};
    assert(similarity_rhythm_score(zero_numerator, 2, pair, 2, &score) == SIMILARITY_SCORE_ERR_INVALID_INPUT);
    assert(similarity_rhythm_score(pair, 2, zero_denominator, 2, &score) == SIMILARITY_SCORE_ERR_INVALID_INPUT);
    assert(similarity_rhythm_score(pair, 0, pair, 2, &score) == SIMILARITY_SCORE_ERR_INVALID_INPUT);
    assert(similarity_rhythm_score(pair, 2, pair, 2, NULL) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
}

static void test_combined_score(void) {
    const long melody_intervals[] = {2, 2, -3, 0};
    const SimilarityDuration melody_rhythm[] = {{1, 4}, {1, 4}, {1, 2}, {1, 4}};
    SimilarityInput a = {melody_intervals, 4, NULL, 0, melody_rhythm, 4};

    /* Identical on every dimension: 1, with each dimension's score as evidence. */
    SimilarityDimensionWeight all[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1},
                                       {SIMILARITY_DIMENSION_RHYTHM, 1, 1},
                                       {SIMILARITY_DIMENSION_CONTOUR, 2, 1}};
    SimilarityModel *model = make_model(all, 3);
    SimilarityBreakdown breakdown;
    assert(similarity_score(model, &a, &a, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 1);
    assert(breakdown.dimension_score_count == 3);
    for (size_t i = 0; i < 3; i++) {
        assert_score(breakdown.dimension_scores[i].score, 1, 1);
    }

    /* Interval 1/2, rhythm 1/3, contour 3/4, weights 1, 1, 2: (1/2 + 1/3 + 2 * 3/4) / 4 = 7/12. */
    const long other_intervals[] = {2, 1, -3, 5};
    const SimilarityDuration other_rhythm[] = {{1, 4}, {1, 2}, {1, 2}, {1, 4}};
    SimilarityInput b = {other_intervals, 4, NULL, 0, other_rhythm, 4};
    assert(similarity_score(model, &a, &b, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 7, 12);
    assert(breakdown.dimension_scores[0].dimension == SIMILARITY_DIMENSION_INTERVAL);
    assert_score(breakdown.dimension_scores[0].score, 1, 2);
    assert(breakdown.dimension_scores[1].dimension == SIMILARITY_DIMENSION_RHYTHM);
    assert_score(breakdown.dimension_scores[1].score, 1, 3);
    assert(breakdown.dimension_scores[2].dimension == SIMILARITY_DIMENSION_CONTOUR);
    assert_score(breakdown.dimension_scores[2].score, 3, 4);

    /* Explicit directions are used when given: the same contour as a's, whatever the intervals say. */
    const ContourDirection same_contour[] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN,
                                             CONTOUR_DIRECTION_SAME};
    SimilarityInput with_directions = {other_intervals, 4, same_contour, 4, other_rhythm, 4};
    assert(similarity_score(model, &a, &with_directions, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.dimension_scores[2].score, 1, 1);
    /* (1/2 + 1/3 + 2 * 1) / 4 = 17/24. */
    assert_score(breakdown.similarity, 17, 24);
    similarity_model_destroy(model);
}

static void test_undeclared_dimensions_contribute_nothing(void) {
    const long melody_intervals[] = {2, 2, -3, 0};
    const long other_intervals[] = {2, 1, -3, 5};
    const SimilarityDuration melody_rhythm[] = {{1, 4}, {1, 4}, {1, 2}, {1, 4}};
    const SimilarityDuration other_rhythm[] = {{1, 4}, {1, 2}, {1, 2}, {1, 4}};
    SimilarityInput a = {melody_intervals, 4, NULL, 0, melody_rhythm, 4};
    SimilarityInput b = {other_intervals, 4, NULL, 0, other_rhythm, 4};
    SimilarityBreakdown breakdown;

    /* Interval only: 1/2, whatever rhythm and contour would say. */
    SimilarityDimensionWeight interval_only[] = {{SIMILARITY_DIMENSION_INTERVAL, 3, 7}};
    SimilarityModel *model = make_model(interval_only, 1);
    assert(similarity_score(model, &a, &b, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 2);
    assert(breakdown.dimension_score_count == 1 && breakdown.dimension_scores[0].dimension == SIMILARITY_DIMENSION_INTERVAL);
    similarity_model_destroy(model);

    /* Its other sequences are not even read: inputs with only intervals score fine. */
    SimilarityInput only_intervals_a = {melody_intervals, 4, NULL, 0, NULL, 0};
    SimilarityInput only_intervals_b = {other_intervals, 4, NULL, 0, NULL, 0};
    model = make_model(interval_only, 1);
    assert(similarity_score(model, &only_intervals_a, &only_intervals_b, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 2);
    similarity_model_destroy(model);

    /* Rhythm only: 1/3. Contour only (derived from the intervals): 3/4. */
    SimilarityDimensionWeight rhythm_only[] = {{SIMILARITY_DIMENSION_RHYTHM, 1, 1}};
    model = make_model(rhythm_only, 1);
    assert(similarity_score(model, &a, &b, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 3);
    similarity_model_destroy(model);
    SimilarityDimensionWeight contour_only[] = {{SIMILARITY_DIMENSION_CONTOUR, 5, 2}};
    model = make_model(contour_only, 1);
    assert(similarity_score(model, &a, &b, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 3, 4);
    similarity_model_destroy(model);

    /* A zero weight is as good as undeclared: counted dimensions only, and the zero one is not read. */
    SimilarityDimensionWeight zero_rhythm[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}, {SIMILARITY_DIMENSION_RHYTHM, 0, 1}};
    model = make_model(zero_rhythm, 2);
    assert(similarity_score(model, &only_intervals_a, &only_intervals_b, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 2);
    assert(breakdown.dimension_score_count == 1);
    similarity_model_destroy(model);
}

static void test_transposition_rhythm_and_extremes_under_a_model(void) {
    SimilarityDimensionWeight all[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1},
                                       {SIMILARITY_DIMENSION_RHYTHM, 1, 1},
                                       {SIMILARITY_DIMENSION_CONTOUR, 1, 1}};
    SimilarityModel *model = make_model(all, 3);
    SimilarityBreakdown breakdown;

    /* A transposed copy: the same intervals and contour, the same rhythm: 1 on all. */
    const long intervals[] = {4, -2, -2, 5};
    const SimilarityDuration rhythm[] = {{1, 4}, {1, 8}, {1, 8}, {1, 2}};
    SimilarityInput original = {intervals, 4, NULL, 0, rhythm, 4};
    SimilarityInput transposed = {intervals, 4, NULL, 0, rhythm, 4};
    assert(similarity_score(model, &original, &transposed, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 1);

    /* A rhythm-augmented copy of the same notes: intervals and contour 1, rhythm 1 too (ratios unchanged). */
    const SimilarityDuration augmented[] = {{1, 2}, {1, 4}, {1, 4}, {1, 1}};
    SimilarityInput augmented_copy = {intervals, 4, NULL, 0, augmented, 4};
    assert(similarity_score(model, &original, &augmented_copy, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 1, 1);

    /* Another rhythm and other intervals and contour: rhythm differs, interval and contour as scored. */
    const SimilarityDuration other_rhythm[] = {{1, 8}, {1, 4}, {1, 2}, {1, 4}}; /* ratios 2, 2, 1/2 against 1/2, 1, 4 */
    SimilarityInput same_notes_other_rhythm = {intervals, 4, NULL, 0, other_rhythm, 4};
    assert(similarity_score(model, &original, &same_notes_other_rhythm, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.dimension_scores[0].score, 1, 1);
    assert_score(breakdown.dimension_scores[1].score, 0, 1);
    assert_score(breakdown.dimension_scores[2].score, 1, 1);
    assert_score(breakdown.similarity, 2, 3);

    /* Different at every position of every dimension: 0. */
    const long up[] = {1, 1, 1};
    const long down[] = {-1, -1, -1};
    const SimilarityDuration steady[] = {{1, 4}, {1, 4}, {1, 4}};
    const SimilarityDuration uneven[] = {{1, 4}, {1, 2}, {1, 4}};
    SimilarityInput rising = {up, 3, NULL, 0, steady, 3};
    SimilarityInput falling = {down, 3, NULL, 0, uneven, 3};
    assert(similarity_score(model, &rising, &falling, &breakdown) == SIMILARITY_SCORE_OK);
    assert_score(breakdown.similarity, 0, 1);

    /* A counted dimension with nothing to compare is invalid input; missing arguments are reported. The output is
       untouched on error. */
    SimilarityInput no_rhythm = {intervals, 4, NULL, 0, NULL, 0};
    breakdown.similarity.numerator = 7;
    breakdown.similarity.denominator = 7;
    assert(similarity_score(model, &original, &no_rhythm, &breakdown) == SIMILARITY_SCORE_ERR_INVALID_INPUT);
    assert_score(breakdown.similarity, 7, 7);
    assert(similarity_score(NULL, &original, &original, &breakdown) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
    assert(similarity_score(model, NULL, &original, &breakdown) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
    assert(similarity_score(model, &original, NULL, &breakdown) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
    assert(similarity_score(model, &original, &original, NULL) == SIMILARITY_SCORE_ERR_NULL_ARGUMENT);
    similarity_model_destroy(model);
}

int main(void) {
    test_interval_dimension();
    test_contour_dimension();
    test_rhythm_dimension();
    test_combined_score();
    test_undeclared_dimensions_contribute_nothing();
    test_transposition_rhythm_and_extremes_under_a_model();
    printf("test_similarity_scoring: ok\n");
    return 0;
}
