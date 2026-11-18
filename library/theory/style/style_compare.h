#ifndef URSATZ_THEORY_STYLE_COMPARE_H
#define URSATZ_THEORY_STYLE_COMPARE_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "theory/style/style_labels.h"
#include "theory/style/style_model.h"

/*
 * Comparison of two style models (R-617). Per category the distance is the total variation distance between the two
 * models' label frequencies: half the sum over labels of |count_a/total_a - count_b/total_b|, an exact reduced Rational in
 * [0, 1]. A category only one model has is distance 1. A dimension's distance is the mean of its categories' and the
 * overall distance is the mean of the dimensions that have a category in either model, both as doubles computed from the
 * exact category distances.
 */

typedef struct {
    char *category;
    StyleDimension dimension;
    Rational distance;
    bool in_a, in_b;  /* which models have the category */
    char *top_a[3];   /* each model's most frequent labels, most frequent first; NULL after the last */
    char *top_b[3];
} StyleCategoryDistance;

typedef struct {
    StyleCategoryDistance *categories; /* sorted by category */
    size_t category_count;
    bool dimension_present[STYLE_DIMENSION_COUNT];
    double dimension[STYLE_DIMENSION_COUNT];
    bool overall_present;
    double overall;
} StyleComparison;

typedef enum {
    STYLE_COMPARE_OK = 0,
    STYLE_COMPARE_ERR_ARGUMENT,
    STYLE_COMPARE_ERR_MODEL,    /* a model's statistics cannot be read or name a category this module does not define */
    STYLE_COMPARE_ERR_OVERFLOW, /* an exact category distance does not fit; never wrapped */
    STYLE_COMPARE_ERR_ALLOC
} StyleCompareResult;

StyleCompareResult style_profile_compare(const StyleModel *a, const StyleModel *b, StyleComparison *out);
void style_comparison_destroy(StyleComparison *comparison);

#endif
