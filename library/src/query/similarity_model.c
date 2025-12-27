#include "query/similarity_model.h"

#include <stdlib.h>

#include "query/query_fraction.h"

struct SimilarityModel {
    size_t count;
    SimilarityDimension dimensions[SIMILARITY_DIMENSION_COUNT];
    QueryFraction weights[SIMILARITY_DIMENSION_COUNT];
    QueryFraction total;
};

const char *similarity_dimension_name(SimilarityDimension dimension) {
    switch (dimension) {
        case SIMILARITY_DIMENSION_INTERVAL:
            return "interval";
        case SIMILARITY_DIMENSION_RHYTHM:
            return "rhythm";
        case SIMILARITY_DIMENSION_CONTOUR:
            return "contour";
        default:
            return NULL;
    }
}

SimilarityModelResult similarity_model_create(const SimilarityDimensionWeight *weights, size_t count,
                                              SimilarityModel **out) {
    if (out == NULL || (count > 0 && weights == NULL)) {
        return SIMILARITY_MODEL_ERR_NULL_ARGUMENT;
    }
    if (count == 0) {
        return SIMILARITY_MODEL_ERR_EMPTY_DIMENSIONS;
    }
    if (count > SIMILARITY_DIMENSION_COUNT) {
        return SIMILARITY_MODEL_ERR_DUPLICATE_DIMENSION; /* more entries than dimensions exist */
    }

    QueryFraction parsed[SIMILARITY_DIMENSION_COUNT];
    QueryFraction total = {0, 1};
    int declared[SIMILARITY_DIMENSION_COUNT] = {0};
    for (size_t i = 0; i < count; i++) {
        SimilarityDimension dimension = weights[i].dimension;
        if ((int)dimension < 0 || dimension >= SIMILARITY_DIMENSION_COUNT) {
            return SIMILARITY_MODEL_ERR_UNKNOWN_DIMENSION;
        }
        if (declared[dimension]) {
            return SIMILARITY_MODEL_ERR_DUPLICATE_DIMENSION;
        }
        declared[dimension] = 1;
        if (weights[i].weight_denominator == 0) {
            return SIMILARITY_MODEL_ERR_ZERO_DENOMINATOR;
        }
        if (weights[i].weight_numerator < 0 || weights[i].weight_denominator < 0) {
            return SIMILARITY_MODEL_ERR_NEGATIVE_WEIGHT;
        }
        QueryFraction weight = {(unsigned long)weights[i].weight_numerator, (unsigned long)weights[i].weight_denominator};
        if (query_fraction_reduce(weight, &parsed[i]) != QUERY_FRACTION_OK ||
            query_fraction_add(total, parsed[i], &total) != QUERY_FRACTION_OK) {
            return SIMILARITY_MODEL_ERR_WEIGHT_OVERFLOW;
        }
    }
    if (total.numerator == 0) {
        return SIMILARITY_MODEL_ERR_ZERO_TOTAL_WEIGHT;
    }

    SimilarityModel *model = (SimilarityModel *)calloc(1, sizeof(*model));
    if (model == NULL) {
        return SIMILARITY_MODEL_ERR_ALLOC;
    }
    model->count = count;
    for (size_t i = 0; i < count; i++) {
        model->dimensions[i] = weights[i].dimension;
        model->weights[i] = parsed[i];
    }
    model->total = total;
    *out = model;
    return SIMILARITY_MODEL_OK;
}

void similarity_model_destroy(SimilarityModel *model) {
    free(model);
}

size_t similarity_model_dimension_count(const SimilarityModel *model) {
    return model == NULL ? 0 : model->count;
}

int similarity_model_dimension_at(const SimilarityModel *model, size_t index, SimilarityDimension *out_dimension,
                                  unsigned long *out_weight_numerator, unsigned long *out_weight_denominator) {
    if (model == NULL || index >= model->count) {
        return 0;
    }
    if (out_dimension != NULL) {
        *out_dimension = model->dimensions[index];
    }
    if (out_weight_numerator != NULL) {
        *out_weight_numerator = model->weights[index].numerator;
    }
    if (out_weight_denominator != NULL) {
        *out_weight_denominator = model->weights[index].denominator;
    }
    return 1;
}

int similarity_model_declares(const SimilarityModel *model, SimilarityDimension dimension) {
    return similarity_model_weight(model, dimension, NULL, NULL);
}

int similarity_model_weight(const SimilarityModel *model, SimilarityDimension dimension,
                            unsigned long *out_numerator, unsigned long *out_denominator) {
    if (model == NULL) {
        return 0;
    }
    for (size_t i = 0; i < model->count; i++) {
        if (model->dimensions[i] == dimension) {
            if (out_numerator != NULL) {
                *out_numerator = model->weights[i].numerator;
            }
            if (out_denominator != NULL) {
                *out_denominator = model->weights[i].denominator;
            }
            return 1;
        }
    }
    return 0;
}

void similarity_model_total_weight(const SimilarityModel *model, unsigned long *out_numerator,
                                   unsigned long *out_denominator) {
    unsigned long numerator = model == NULL ? 0 : model->total.numerator;
    unsigned long denominator = model == NULL ? 1 : model->total.denominator;
    if (out_numerator != NULL) {
        *out_numerator = numerator;
    }
    if (out_denominator != NULL) {
        *out_denominator = denominator;
    }
}
