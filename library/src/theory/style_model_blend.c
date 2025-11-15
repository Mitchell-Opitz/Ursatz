#include "theory/style_model_blend.h"

#include <stdlib.h>
#include <string.h>

#include "theory/style_features.h"
#include "theory/style_fraction.h"
#include "theory/style_model_build.h"

static int compare_keys(const StyleStatistic *a, const StyleStatistic *b) {
    int by_category = strcmp(a->category, b->category);
    return by_category != 0 ? by_category : strcmp(a->label, b->label);
}

static StyleBlendResult from_fraction_result(StyleFractionResult status) {
    switch (status) {
        case STYLE_FRACTION_OK:
            return STYLE_BLEND_OK;
        case STYLE_FRACTION_ERR_OVERFLOW:
            return STYLE_BLEND_ERR_OVERFLOW;
        default:
            return STYLE_BLEND_ERR_MALFORMED;
    }
}

/* weight_a * frequency_a + weight_b * frequency_b; a missing statistic (NULL) has frequency 0. */
static StyleBlendResult blend_frequency(const StyleStatistic *a, const StyleStatistic *b, const StyleFraction *weight_a,
                                        const StyleFraction *weight_b, StyleFraction *out) {
    StyleFraction zero = {0, 1};
    StyleFraction frequency_a = a == NULL ? zero : (StyleFraction){a->count, a->total};
    StyleFraction frequency_b = b == NULL ? zero : (StyleFraction){b->count, b->total};
    StyleFraction left, right;
    StyleBlendResult status = from_fraction_result(style_fraction_multiply(weight_a, &frequency_a, &left));
    if (status == STYLE_BLEND_OK) {
        status = from_fraction_result(style_fraction_multiply(weight_b, &frequency_b, &right));
    }
    if (status == STYLE_BLEND_OK) {
        status = from_fraction_result(style_fraction_add(&left, &right, out));
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
    StyleFraction weight_b = {weight_numerator, weight_denominator};
    StyleFraction weight_a = {weight_denominator - weight_numerator, weight_denominator};

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

        StyleFraction frequency;
        result = blend_frequency(from_a, from_b, &weight_a, &weight_b, &frequency);
        if (result == STYLE_BLEND_OK && frequency.numerator > 0) {
            blended[blended_count].category = key->category;
            blended[blended_count].label = key->label;
            blended[blended_count].count = frequency.numerator;
            blended[blended_count].total = frequency.denominator;
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
