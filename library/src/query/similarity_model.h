#ifndef MUSIC_QUERY_SIMILARITY_MODEL_H
#define MUSIC_QUERY_SIMILARITY_MODEL_H

#include <stddef.h>

/*
 * SimilarityModel (music-query, Registry "Query"): which dimensions count
 * when two sequences are compared, and how much each counts. Similarity is
 * explicit-dimension: the caller declares the dimensions (interval, rhythm,
 * contour) and an exact-rational weight for each, and a dimension that is not
 * declared contributes nothing -- there is no undeclared similarity. See
 * query/similarity_scoring.h for what each dimension measures.
 *
 * A weight is numerator/denominator, an exact rational (never float/double).
 * Both are signed so that a negative weight is an explicit error rather than
 * unrepresentable. A zero weight is allowed (it contributes nothing, as if the
 * dimension were not declared) but the weights must not all be zero. Weights
 * are stored in lowest terms, and their sum is checked at creation: a model
 * whose weights cannot be summed without overflow is rejected, so scoring
 * never has to wrap.
 *
 * Similarity is a descriptive match, never a framework-validated claim, and
 * nothing here is or becomes a Rule.
 */

typedef struct SimilarityModel SimilarityModel;

typedef enum {
    SIMILARITY_DIMENSION_INTERVAL = 0,
    SIMILARITY_DIMENSION_RHYTHM,
    SIMILARITY_DIMENSION_CONTOUR,
    SIMILARITY_DIMENSION_COUNT
} SimilarityDimension;

typedef struct {
    SimilarityDimension dimension;
    long weight_numerator;
    long weight_denominator;
} SimilarityDimensionWeight;

typedef enum {
    SIMILARITY_MODEL_OK = 0,
    SIMILARITY_MODEL_ERR_NULL_ARGUMENT,
    SIMILARITY_MODEL_ERR_EMPTY_DIMENSIONS,
    SIMILARITY_MODEL_ERR_UNKNOWN_DIMENSION,
    SIMILARITY_MODEL_ERR_DUPLICATE_DIMENSION,
    SIMILARITY_MODEL_ERR_ZERO_DENOMINATOR,
    SIMILARITY_MODEL_ERR_NEGATIVE_WEIGHT, /* a negative numerator or denominator */
    SIMILARITY_MODEL_ERR_ZERO_TOTAL_WEIGHT,
    SIMILARITY_MODEL_ERR_WEIGHT_OVERFLOW, /* the weights' exact sum does not fit in unsigned long */
    SIMILARITY_MODEL_ERR_ALLOC
} SimilarityModelResult;

/*
 * A new model the caller owns, with the given dimensions (weights is copied).
 * *out is untouched on any result other than OK. The dimension set must be
 * non-empty, each dimension a known one and declared once, each weight a
 * non-negative fraction with a positive denominator, and the total weight
 * positive and summable.
 */
SimilarityModelResult similarity_model_create(const SimilarityDimensionWeight *weights, size_t count,
                                              SimilarityModel **out);

/* Safe on NULL. */
void similarity_model_destroy(SimilarityModel *model);

/* How many dimensions are declared (0 for NULL). */
size_t similarity_model_dimension_count(const SimilarityModel *model);

/*
 * The index-th declared dimension, in the order given at creation, with its
 * weight in lowest terms. Returns 0 and leaves the outputs untouched if the
 * index is out of range; 1 otherwise.
 */
int similarity_model_dimension_at(const SimilarityModel *model, size_t index, SimilarityDimension *out_dimension,
                                  unsigned long *out_weight_numerator, unsigned long *out_weight_denominator);

/* Whether a dimension is declared, and (if so) its weight in lowest terms; 0 and untouched outputs otherwise. */
int similarity_model_declares(const SimilarityModel *model, SimilarityDimension dimension);
int similarity_model_weight(const SimilarityModel *model, SimilarityDimension dimension,
                            unsigned long *out_numerator, unsigned long *out_denominator);

/* The sum of all weights in lowest terms (0/1 for NULL); never zero for a model that exists. */
void similarity_model_total_weight(const SimilarityModel *model, unsigned long *out_numerator,
                                   unsigned long *out_denominator);

/* A readable name for a dimension: "interval", "rhythm", "contour" (NULL if unknown). */
const char *similarity_dimension_name(SimilarityDimension dimension);

#endif
