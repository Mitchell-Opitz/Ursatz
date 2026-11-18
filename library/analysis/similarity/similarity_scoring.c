#include "analysis/similarity/similarity_scoring.h"

#include <limits.h>
#include <stdlib.h>


/* matches / total, in lowest terms. total is positive. */
static SimilarityScore ratio_of(size_t matches, size_t total) {
    return rational_ratio((long long)matches, (long long)total);
}

SimilarityScoreResult similarity_interval_score(const long *a, size_t a_count, const long *b, size_t b_count,
                                                SimilarityScore *out) {
    if (out == NULL || (a_count > 0 && a == NULL) || (b_count > 0 && b == NULL)) {
        return SIMILARITY_SCORE_ERR_NULL_ARGUMENT;
    }
    if (a_count == 0 || b_count == 0) {
        return SIMILARITY_SCORE_ERR_INVALID_INPUT;
    }
    if (a_count != b_count) {
        *out = ratio_of(0, 1);
        return SIMILARITY_SCORE_OK;
    }
    size_t matches = 0;
    for (size_t i = 0; i < a_count; i++) {
        if (a[i] == b[i]) {
            matches++;
        }
    }
    *out = ratio_of(matches, a_count);
    return SIMILARITY_SCORE_OK;
}

SimilarityScoreResult similarity_contour_score(const ContourDirection *a, size_t a_count, const ContourDirection *b,
                                               size_t b_count, SimilarityScore *out) {
    if (out == NULL || (a_count > 0 && a == NULL) || (b_count > 0 && b == NULL)) {
        return SIMILARITY_SCORE_ERR_NULL_ARGUMENT;
    }
    if (a_count == 0 || b_count == 0) {
        return SIMILARITY_SCORE_ERR_INVALID_INPUT;
    }
    if (a_count != b_count) {
        *out = ratio_of(0, 1);
        return SIMILARITY_SCORE_OK;
    }
    size_t matches = 0;
    for (size_t i = 0; i < a_count; i++) {
        if (a[i] == b[i]) {
            matches++;
        }
    }
    *out = ratio_of(matches, a_count);
    return SIMILARITY_SCORE_OK;
}

/* d[i+1] / d[i] as an exact fraction. */
static RationalResult consecutive_ratio(const SimilarityDuration *durations, size_t i, Rational *out) {
    return rational_div(durations[i + 1], durations[i], out);
}

SimilarityScoreResult similarity_rhythm_score(const SimilarityDuration *a, size_t a_count,
                                              const SimilarityDuration *b, size_t b_count, SimilarityScore *out) {
    if (out == NULL || (a_count > 0 && a == NULL) || (b_count > 0 && b == NULL)) {
        return SIMILARITY_SCORE_ERR_NULL_ARGUMENT;
    }
    if (a_count == 0 || b_count == 0) {
        return SIMILARITY_SCORE_ERR_INVALID_INPUT;
    }
    for (size_t i = 0; i < a_count; i++) {
        if (a[i].num <= 0 || a[i].den <= 0) {
            return SIMILARITY_SCORE_ERR_INVALID_INPUT;
        }
    }
    for (size_t i = 0; i < b_count; i++) {
        if (b[i].num <= 0 || b[i].den <= 0) {
            return SIMILARITY_SCORE_ERR_INVALID_INPUT;
        }
    }
    if (a_count != b_count) {
        *out = ratio_of(0, 1);
        return SIMILARITY_SCORE_OK;
    }
    if (a_count == 1) {
        *out = ratio_of(1, 1);
        return SIMILARITY_SCORE_OK;
    }
    size_t matches = 0;
    for (size_t i = 0; i + 1 < a_count; i++) {
        Rational ratio_a, ratio_b;
        if (consecutive_ratio(a, i, &ratio_a) != RATIONAL_OK || consecutive_ratio(b, i, &ratio_b) != RATIONAL_OK) {
            return SIMILARITY_SCORE_ERR_OVERFLOW;
        }
        if (rational_cmp(ratio_a, ratio_b) == 0) {
            matches++;
        }
    }
    *out = ratio_of(matches, a_count - 1);
    return SIMILARITY_SCORE_OK;
}

void similarity_directions_from_intervals(const long *intervals, size_t count, ContourDirection *out) {
    for (size_t i = 0; i < count; i++) {
        out[i] = intervals[i] > 0 ? CONTOUR_DIRECTION_UP : (intervals[i] < 0 ? CONTOUR_DIRECTION_DOWN : CONTOUR_DIRECTION_SAME);
    }
}

/* An input's directions: its own array, or derived from its intervals into `buffer`. */
static const ContourDirection *directions_of(const SimilarityInput *input, ContourDirection **buffer, size_t *count) {
    *buffer = NULL;
    if (input->direction_count > 0) {
        *count = input->direction_count;
        return input->directions;
    }
    *count = input->interval_count;
    if (input->interval_count == 0 || input->intervals == NULL) {
        *count = 0;
        return NULL;
    }
    *buffer = (ContourDirection *)malloc(input->interval_count * sizeof(ContourDirection));
    if (*buffer == NULL) {
        return NULL;
    }
    similarity_directions_from_intervals(input->intervals, input->interval_count, *buffer);
    return *buffer;
}

SimilarityScoreResult similarity_score(const SimilarityModel *model, const SimilarityInput *a, const SimilarityInput *b,
                                       SimilarityBreakdown *out) {
    if (model == NULL || a == NULL || b == NULL || out == NULL) {
        return SIMILARITY_SCORE_ERR_NULL_ARGUMENT;
    }

    SimilarityBreakdown breakdown;
    breakdown.dimension_score_count = 0;
    Rational sum = {0, 1};
    SimilarityScoreResult status = SIMILARITY_SCORE_OK;

    for (size_t i = 0; i < similarity_model_dimension_count(model) && status == SIMILARITY_SCORE_OK; i++) {
        SimilarityDimension dimension;
        unsigned long weight_numerator, weight_denominator;
        similarity_model_dimension_at(model, i, &dimension, &weight_numerator, &weight_denominator);
        if (weight_numerator == 0) {
            continue; /* weight 0: contributes nothing, and its sequence is not read */
        }

        SimilarityScore score = {0, 1};
        if (dimension == SIMILARITY_DIMENSION_INTERVAL) {
            status = similarity_interval_score(a->intervals, a->interval_count, b->intervals, b->interval_count, &score);
        } else if (dimension == SIMILARITY_DIMENSION_RHYTHM) {
            status = similarity_rhythm_score(a->durations, a->duration_count, b->durations, b->duration_count, &score);
        } else {
            ContourDirection *buffer_a = NULL, *buffer_b = NULL;
            size_t count_a = 0, count_b = 0;
            const ContourDirection *directions_a = directions_of(a, &buffer_a, &count_a);
            const ContourDirection *directions_b = directions_of(b, &buffer_b, &count_b);
            if ((a->direction_count == 0 && a->interval_count > 0 && buffer_a == NULL) ||
                (b->direction_count == 0 && b->interval_count > 0 && buffer_b == NULL)) {
                status = SIMILARITY_SCORE_ERR_ALLOC;
            } else {
                status = similarity_contour_score(directions_a, count_a, directions_b, count_b, &score);
            }
            free(buffer_a);
            free(buffer_b);
        }
        if (status != SIMILARITY_SCORE_OK) {
            break;
        }

        Rational weight;
        Rational term;
        if (weight_numerator > LLONG_MAX || weight_denominator > LLONG_MAX ||
            rational_make((long long)weight_numerator, (long long)weight_denominator, &weight) != RATIONAL_OK ||
            rational_mul(weight, score, &term) != RATIONAL_OK || rational_add(sum, term, &sum) != RATIONAL_OK) {
            status = SIMILARITY_SCORE_ERR_OVERFLOW;
            break;
        }
        breakdown.dimension_scores[breakdown.dimension_score_count].dimension = dimension;
        breakdown.dimension_scores[breakdown.dimension_score_count].score = score;
        breakdown.dimension_score_count++;
    }
    if (status != SIMILARITY_SCORE_OK) {
        return status;
    }

    /* Dividing by the total weight is multiplying by its reciprocal. The total counts zero-weight dimensions as 0. */
    unsigned long total_numerator, total_denominator;
    similarity_model_total_weight(model, &total_numerator, &total_denominator);
    Rational reciprocal;
    Rational combined;
    if (total_numerator > LLONG_MAX || total_denominator > LLONG_MAX ||
        rational_make((long long)total_denominator, (long long)total_numerator, &reciprocal) != RATIONAL_OK ||
        rational_mul(sum, reciprocal, &combined) != RATIONAL_OK) {
        return SIMILARITY_SCORE_ERR_OVERFLOW;
    }
    breakdown.similarity = combined;
    *out = breakdown;
    return SIMILARITY_SCORE_OK;
}
