#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "query/similarity_model.h"

/*
 * A SimilarityModel declares dimensions with exact-rational weights and
 * rejects what cannot be scored: an empty dimension set, a zero denominator,
 * negative weights, weights that all vanish, unknown or repeated dimensions,
 * and weights whose sum overflows.
 */

static SimilarityModelResult create(const SimilarityDimensionWeight *weights, size_t count) {
    SimilarityModel *model = (SimilarityModel *)0x1;
    SimilarityModelResult result = similarity_model_create(weights, count, &model);
    if (result == SIMILARITY_MODEL_OK) {
        similarity_model_destroy(model);
    } else {
        assert(model == (SimilarityModel *)0x1); /* untouched on any error */
    }
    return result;
}

static void test_a_valid_model(void) {
    /* Weights are kept in lowest terms; the order is the order given. */
    const SimilarityDimensionWeight weights[] = {{SIMILARITY_DIMENSION_CONTOUR, 2, 4},
                                                 {SIMILARITY_DIMENSION_INTERVAL, 3, 1},
                                                 {SIMILARITY_DIMENSION_RHYTHM, 0, 9}};
    SimilarityModel *model = NULL;
    assert(similarity_model_create(weights, 3, &model) == SIMILARITY_MODEL_OK);
    assert(similarity_model_dimension_count(model) == 3);

    SimilarityDimension dimension;
    unsigned long numerator = 99, denominator = 99;
    assert(similarity_model_dimension_at(model, 0, &dimension, &numerator, &denominator));
    assert(dimension == SIMILARITY_DIMENSION_CONTOUR && numerator == 1 && denominator == 2);
    assert(similarity_model_dimension_at(model, 1, &dimension, &numerator, &denominator));
    assert(dimension == SIMILARITY_DIMENSION_INTERVAL && numerator == 3 && denominator == 1);
    assert(similarity_model_dimension_at(model, 2, &dimension, &numerator, &denominator));
    assert(dimension == SIMILARITY_DIMENSION_RHYTHM && numerator == 0 && denominator == 1);
    assert(!similarity_model_dimension_at(model, 3, &dimension, &numerator, &denominator));
    assert(similarity_model_dimension_at(model, 0, NULL, NULL, NULL));

    assert(similarity_model_declares(model, SIMILARITY_DIMENSION_INTERVAL));
    assert(similarity_model_declares(model, SIMILARITY_DIMENSION_RHYTHM));
    assert(similarity_model_weight(model, SIMILARITY_DIMENSION_CONTOUR, &numerator, &denominator));
    assert(numerator == 1 && denominator == 2);

    /* 1/2 + 3 + 0 = 7/2. */
    similarity_model_total_weight(model, &numerator, &denominator);
    assert(numerator == 7 && denominator == 2);
    similarity_model_destroy(model);

    /* A dimension not declared is not there. */
    const SimilarityDimensionWeight only_interval[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    assert(similarity_model_create(only_interval, 1, &model) == SIMILARITY_MODEL_OK);
    assert(similarity_model_dimension_count(model) == 1);
    assert(!similarity_model_declares(model, SIMILARITY_DIMENSION_RHYTHM));
    assert(!similarity_model_declares(model, SIMILARITY_DIMENSION_CONTOUR));
    numerator = 99;
    assert(!similarity_model_weight(model, SIMILARITY_DIMENSION_RHYTHM, &numerator, NULL));
    assert(numerator == 99);
    similarity_model_destroy(model);

    /* NULL is a model with no dimensions. */
    assert(similarity_model_dimension_count(NULL) == 0);
    assert(!similarity_model_declares(NULL, SIMILARITY_DIMENSION_INTERVAL));
    assert(!similarity_model_dimension_at(NULL, 0, NULL, NULL, NULL));
    similarity_model_total_weight(NULL, &numerator, &denominator);
    assert(numerator == 0 && denominator == 1);
    similarity_model_destroy(NULL);

    assert(strcmp(similarity_dimension_name(SIMILARITY_DIMENSION_INTERVAL), "interval") == 0);
    assert(strcmp(similarity_dimension_name(SIMILARITY_DIMENSION_RHYTHM), "rhythm") == 0);
    assert(strcmp(similarity_dimension_name(SIMILARITY_DIMENSION_CONTOUR), "contour") == 0);
    assert(similarity_dimension_name(SIMILARITY_DIMENSION_COUNT) == NULL);
}

static void test_invalid_models(void) {
    const SimilarityDimensionWeight one[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1}};

    /* No dimensions, and missing arguments. */
    assert(create(one, 0) == SIMILARITY_MODEL_ERR_EMPTY_DIMENSIONS);
    assert(create(NULL, 0) == SIMILARITY_MODEL_ERR_EMPTY_DIMENSIONS);
    assert(create(NULL, 1) == SIMILARITY_MODEL_ERR_NULL_ARGUMENT);
    assert(similarity_model_create(one, 1, NULL) == SIMILARITY_MODEL_ERR_NULL_ARGUMENT);

    /* A zero denominator. */
    const SimilarityDimensionWeight zero_denominator[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 0}};
    assert(create(zero_denominator, 1) == SIMILARITY_MODEL_ERR_ZERO_DENOMINATOR);
    const SimilarityDimensionWeight zero_over_zero[] = {{SIMILARITY_DIMENSION_INTERVAL, 0, 0}};
    assert(create(zero_over_zero, 1) == SIMILARITY_MODEL_ERR_ZERO_DENOMINATOR);

    /* Negative weights, in either part. */
    const SimilarityDimensionWeight negative_numerator[] = {{SIMILARITY_DIMENSION_INTERVAL, -1, 2}};
    const SimilarityDimensionWeight negative_denominator[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, -2}};
    const SimilarityDimensionWeight later_negative[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1},
                                                        {SIMILARITY_DIMENSION_RHYTHM, -3, 1}};
    assert(create(negative_numerator, 1) == SIMILARITY_MODEL_ERR_NEGATIVE_WEIGHT);
    assert(create(negative_denominator, 1) == SIMILARITY_MODEL_ERR_NEGATIVE_WEIGHT);
    assert(create(later_negative, 2) == SIMILARITY_MODEL_ERR_NEGATIVE_WEIGHT);

    /* Weights that are all zero count for nothing. */
    const SimilarityDimensionWeight all_zero[] = {{SIMILARITY_DIMENSION_INTERVAL, 0, 1}, {SIMILARITY_DIMENSION_RHYTHM, 0, 3}};
    assert(create(all_zero, 2) == SIMILARITY_MODEL_ERR_ZERO_TOTAL_WEIGHT);
    const SimilarityDimensionWeight single_zero[] = {{SIMILARITY_DIMENSION_CONTOUR, 0, 1}};
    assert(create(single_zero, 1) == SIMILARITY_MODEL_ERR_ZERO_TOTAL_WEIGHT);

    /* An unknown dimension, and a repeated one. */
    const SimilarityDimensionWeight unknown[] = {{(SimilarityDimension)42, 1, 1}};
    const SimilarityDimensionWeight negative_dimension[] = {{(SimilarityDimension)-1, 1, 1}};
    const SimilarityDimensionWeight count_dimension[] = {{SIMILARITY_DIMENSION_COUNT, 1, 1}};
    const SimilarityDimensionWeight repeated[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1},
                                                  {SIMILARITY_DIMENSION_INTERVAL, 2, 1}};
    assert(create(unknown, 1) == SIMILARITY_MODEL_ERR_UNKNOWN_DIMENSION);
    assert(create(negative_dimension, 1) == SIMILARITY_MODEL_ERR_UNKNOWN_DIMENSION);
    assert(create(count_dimension, 1) == SIMILARITY_MODEL_ERR_UNKNOWN_DIMENSION);
    assert(create(repeated, 2) == SIMILARITY_MODEL_ERR_DUPLICATE_DIMENSION);
    const SimilarityDimensionWeight too_many[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, 1},
                                                  {SIMILARITY_DIMENSION_RHYTHM, 1, 1},
                                                  {SIMILARITY_DIMENSION_CONTOUR, 1, 1},
                                                  {SIMILARITY_DIMENSION_INTERVAL, 1, 1}};
    assert(create(too_many, 4) == SIMILARITY_MODEL_ERR_DUPLICATE_DIMENSION);
}

static void test_weights_that_cannot_be_summed(void) {
    /* Two LONG_MAX weights sum to 2 * LONG_MAX, which fits; three do not. */
    const SimilarityDimensionWeight two_big[] = {{SIMILARITY_DIMENSION_INTERVAL, LONG_MAX, 1},
                                                 {SIMILARITY_DIMENSION_RHYTHM, LONG_MAX, 1}};
    assert(create(two_big, 2) == SIMILARITY_MODEL_OK);
    const SimilarityDimensionWeight three_big[] = {{SIMILARITY_DIMENSION_INTERVAL, LONG_MAX, 1},
                                                   {SIMILARITY_DIMENSION_RHYTHM, LONG_MAX, 1},
                                                   {SIMILARITY_DIMENSION_CONTOUR, LONG_MAX, 1}};
    assert(create(three_big, 3) == SIMILARITY_MODEL_ERR_WEIGHT_OVERFLOW);

    /* Two thin fractions whose common denominator does not fit. */
    const SimilarityDimensionWeight thin[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, LONG_MAX},
                                              {SIMILARITY_DIMENSION_RHYTHM, 1, LONG_MAX - 1}};
    assert(create(thin, 2) == SIMILARITY_MODEL_ERR_WEIGHT_OVERFLOW);

    /* The same denominators with a common factor sum exactly. */
    const SimilarityDimensionWeight even[] = {{SIMILARITY_DIMENSION_INTERVAL, 1, LONG_MAX},
                                              {SIMILARITY_DIMENSION_RHYTHM, 1, LONG_MAX}};
    SimilarityModel *model = NULL;
    assert(similarity_model_create(even, 2, &model) == SIMILARITY_MODEL_OK);
    unsigned long numerator, denominator;
    similarity_model_total_weight(model, &numerator, &denominator);
    assert(numerator == 2 && denominator == (unsigned long)LONG_MAX);
    similarity_model_destroy(model);
}

int main(void) {
    test_a_valid_model();
    test_invalid_models();
    test_weights_that_cannot_be_summed();
    printf("test_similarity_model: ok\n");
    return 0;
}
