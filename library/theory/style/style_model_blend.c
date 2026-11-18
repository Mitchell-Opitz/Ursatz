#include "theory/style/style_model_blend.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "theory/style/style_features.h"
#include "model/core/rational.h"
#include "theory/style/style_model_build.h"

static int compare_keys(const StyleStatistic *a, const StyleStatistic *b) {
    int by_category = strcmp(a->category, b->category);
    return by_category != 0 ? by_category : strcmp(a->label, b->label);
}

static StyleBlendResult from_rational_result(RationalResult status) {
    switch (status) {
        case RATIONAL_OK:
            return STYLE_BLEND_OK;
        case RATIONAL_ERR_OVERFLOW:
            return STYLE_BLEND_ERR_OVERFLOW;
        default:
            return STYLE_BLEND_ERR_MALFORMED;
    }
}

/* count / total of a statistic, or 0 for a missing one. */
static RationalResult frequency_of(const StyleStatistic *statistic, Rational *out) {
    if (statistic == NULL) {
        *out = rational_ratio(0, 1);
        return RATIONAL_OK;
    }
    if (statistic->count > (unsigned long)LLONG_MAX || statistic->total > (unsigned long)LLONG_MAX) {
        return RATIONAL_ERR_OVERFLOW;
    }
    return rational_make((long long)statistic->count, (long long)statistic->total, out);
}

/* weight_a * frequency_a + weight_b * frequency_b; a missing statistic (NULL) has frequency 0. */
static StyleBlendResult blend_frequency(const StyleStatistic *a, const StyleStatistic *b, Rational weight_a,
                                        Rational weight_b, Rational *out) {
    Rational frequency_a, frequency_b, left, right;
    StyleBlendResult status = from_rational_result(frequency_of(a, &frequency_a));
    if (status == STYLE_BLEND_OK) {
        status = from_rational_result(frequency_of(b, &frequency_b));
    }
    if (status == STYLE_BLEND_OK) {
        status = from_rational_result(rational_mul(weight_a, frequency_a, &left));
    }
    if (status == STYLE_BLEND_OK) {
        status = from_rational_result(rational_mul(weight_b, frequency_b, &right));
    }
    if (status == STYLE_BLEND_OK) {
        status = from_rational_result(rational_add(left, right, out));
    }
    return status;
}

StyleBlendResult style_model_blend(EntityID *id, const char *style_name, const char *training_corpus,
                                   const char *model_version, const StyleModel *a, const StyleModel *b,
                                   unsigned long weight_numerator, unsigned long weight_denominator,
                                   StyleModel **out) {
    if (id == NULL || a == NULL || b == NULL || out == NULL) {
        return STYLE_BLEND_ERR_NULL_ARGUMENT;
    }
    if (weight_denominator == 0) {
        return STYLE_BLEND_ERR_WEIGHT_ZERO_DENOMINATOR;
    }
    if (weight_numerator > weight_denominator) {
        return STYLE_BLEND_ERR_WEIGHT_OUT_OF_RANGE;
    }

    StyleStatistic *statistics_a = NULL, *statistics_b = NULL;
    size_t count_a = 0, count_b = 0;
    StyleFeaturesResult decoded = style_features_decode(style_model_style_features(a), &statistics_a, &count_a);
    if (decoded != STYLE_FEATURES_OK) {
        return decoded == STYLE_FEATURES_ERR_ALLOC ? STYLE_BLEND_ERR_ALLOC : STYLE_BLEND_ERR_MALFORMED;
    }
    decoded = style_features_decode(style_model_style_features(b), &statistics_b, &count_b);
    if (decoded != STYLE_FEATURES_OK) {
        style_statistics_free(statistics_a, count_a);
        return decoded == STYLE_FEATURES_ERR_ALLOC ? STYLE_BLEND_ERR_ALLOC : STYLE_BLEND_ERR_MALFORMED;
    }

    /* w and 1 - w, as exact fractions. */
    if (weight_denominator > (unsigned long)LLONG_MAX) {
        style_statistics_free(statistics_a, count_a);
        style_statistics_free(statistics_b, count_b);
        return STYLE_BLEND_ERR_OVERFLOW;
    }
    Rational weight_b = rational_ratio((long long)weight_numerator, (long long)weight_denominator);
    Rational weight_a = rational_ratio((long long)(weight_denominator - weight_numerator), (long long)weight_denominator);

    StyleStatistic *blended = (StyleStatistic *)malloc((count_a + count_b + 1) * sizeof(*blended));
    StyleBlendResult result = blended == NULL ? STYLE_BLEND_ERR_ALLOC : STYLE_BLEND_OK;
    size_t blended_count = 0;
    size_t i = 0, j = 0;
    while (result == STYLE_BLEND_OK && (i < count_a || j < count_b)) {
        const StyleStatistic *from_a = NULL, *from_b = NULL;
        const StyleStatistic *key;
        if (j >= count_b || (i < count_a && compare_keys(&statistics_a[i], &statistics_b[j]) < 0)) {
            from_a = &statistics_a[i++];
            key = from_a;
        } else if (i >= count_a || compare_keys(&statistics_a[i], &statistics_b[j]) > 0) {
            from_b = &statistics_b[j++];
            key = from_b;
        } else {
            from_a = &statistics_a[i++];
            from_b = &statistics_b[j++];
            key = from_a;
        }

        Rational frequency;
        result = blend_frequency(from_a, from_b, weight_a, weight_b, &frequency);
        if (result == STYLE_BLEND_OK && frequency.num > 0) {
            blended[blended_count].category = key->category;
            blended[blended_count].label = key->label;
            blended[blended_count].count = (unsigned long)frequency.num;
            blended[blended_count].total = (unsigned long)frequency.den;
            blended_count++;
        }
    }

    if (result == STYLE_BLEND_OK) {
        switch (style_model_from_statistics(id, style_name, training_corpus, model_version, blended, blended_count,
                                            out)) {
            case STYLE_MODEL_BUILD_OK:
                break;
            case STYLE_MODEL_BUILD_ERR_ALLOC:
                result = STYLE_BLEND_ERR_ALLOC;
                break;
            case STYLE_MODEL_BUILD_ERR_MODEL:
                result = STYLE_BLEND_ERR_MODEL;
                break;
            default:
                result = STYLE_BLEND_ERR_MALFORMED;
                break;
        }
    }

    free(blended);
    style_statistics_free(statistics_a, count_a);
    style_statistics_free(statistics_b, count_b);
    return result;
}
