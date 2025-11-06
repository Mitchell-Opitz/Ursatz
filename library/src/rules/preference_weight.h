#ifndef MUSIC_RULES_PREFERENCE_WEIGHT_H
#define MUSIC_RULES_PREFERENCE_WEIGHT_H

#include <stddef.h>

/*
 * Exact-rational arithmetic for preference scores and weights (numerator/
 * denominator over unsigned long, never float/double -- Law 19, the same
 * representation Constraint's weight and Evidence use).
 *
 * Every function that produces a fraction returns it reduced to lowest terms
 * with a positive denominator. Nothing wraps around silently: an operation
 * whose exact result does not fit in unsigned long returns
 * PREFERENCE_WEIGHT_ERR_OVERFLOW and leaves its output untouched, and a
 * fraction with a zero denominator is PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR.
 * A NULL input or output is PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT.
 */

typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} PreferenceFraction;

typedef enum {
    PREFERENCE_WEIGHT_OK = 0,
    PREFERENCE_WEIGHT_ERR_NULL_ARGUMENT,
    PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR,
    PREFERENCE_WEIGHT_ERR_OVERFLOW
} PreferenceWeightResult;

/* Lowest terms: 2/4 -> 1/2, 0/5 -> 0/1. */
PreferenceWeightResult preference_fraction_reduce(const PreferenceFraction *fraction, PreferenceFraction *out);

PreferenceWeightResult preference_fraction_add(const PreferenceFraction *a, const PreferenceFraction *b,
                                               PreferenceFraction *out);
PreferenceWeightResult preference_fraction_multiply(const PreferenceFraction *a, const PreferenceFraction *b,
                                                    PreferenceFraction *out);

/*
 * Orders two fractions exactly: *out_comparison is -1, 0 or 1 as a is less
 * than, equal to or greater than b. Never overflows, since it compares by
 * continued-fraction expansion instead of cross-multiplying.
 */
PreferenceWeightResult preference_fraction_compare(const PreferenceFraction *a, const PreferenceFraction *b,
                                                   int *out_comparison);

/*
 * The weighted total of count scores: sum(weight[i] * score[i]) / sum(weight[i]).
 * With scores in [0, 1] the total stays in [0, 1]. A weight may be zero; if
 * every weight is zero (or count is 0) the total is 0/1 rather than an error,
 * so a caller ranking by it ties every candidate at the worst score. Each
 * weight and score needs a non-zero denominator.
 */
PreferenceWeightResult preference_fraction_weighted_total(const PreferenceFraction *scores,
                                                          const PreferenceFraction *weights, size_t count,
                                                          PreferenceFraction *out);

#endif
