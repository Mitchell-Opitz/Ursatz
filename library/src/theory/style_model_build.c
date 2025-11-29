#include "theory/style_model_build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StyleModelBuildResult style_model_from_statistics(EntityID *id, const char *style_name, const char *training_corpus,
                                                  const char *model_version, const StyleStatistic *statistics,
                                                  size_t count, StyleModel **out) {
    if (id == NULL || out == NULL || (count > 0 && statistics == NULL)) {
        return STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT;
    }
    Value *features = NULL;
    switch (style_features_encode(statistics, count, &features)) {
        case STYLE_FEATURES_OK:
            break;
        case STYLE_FEATURES_ERR_NULL_ARGUMENT:
            return STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT;
        case STYLE_FEATURES_ERR_DUPLICATE:
            return STYLE_MODEL_BUILD_ERR_DUPLICATE;
        case STYLE_FEATURES_ERR_ALLOC:
            return STYLE_MODEL_BUILD_ERR_ALLOC;
        default:
            return STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC;
    }

    StyleModelResult created = style_model_create(id, style_name, features, training_corpus, model_version, out);
    if (created == STYLE_MODEL_ERR_ALLOC) {
        value_destroy(features);
        return STYLE_MODEL_BUILD_ERR_ALLOC;
    }
    if (created != STYLE_MODEL_OK) {
        value_destroy(features);
        return STYLE_MODEL_BUILD_ERR_MODEL;
    }
    return STYLE_MODEL_BUILD_OK;
}

void style_tendencies_free(Tendency **tendencies, size_t count) {
    if (tendencies == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        tendency_destroy(tendencies[i]);
    }
    free(tendencies);
}

StyleTendenciesResult style_model_tendencies(const StyleModel *model, Tendency ***out, size_t *out_count) {
    if (model == NULL || out == NULL || out_count == NULL) {
        return STYLE_TENDENCIES_ERR_NULL_ARGUMENT;
    }
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleFeaturesResult decoded = style_features_decode(style_model_style_features(model), &statistics, &count);
    if (decoded == STYLE_FEATURES_ERR_ALLOC) {
        return STYLE_TENDENCIES_ERR_ALLOC;
    }
    if (decoded != STYLE_FEATURES_OK) {
        return STYLE_TENDENCIES_ERR_MALFORMED;
    }

    Tendency **tendencies = NULL;
    if (count > 0) {
        tendencies = (Tendency **)calloc(count, sizeof(*tendencies));
        if (tendencies == NULL) {
            style_statistics_free(statistics, count);
            return STYLE_TENDENCIES_ERR_ALLOC;
        }
    }

    const char *corpus = style_model_training_corpus(model);
    StyleTendenciesResult result = STYLE_TENDENCIES_OK;
    for (size_t i = 0; i < count && result == STYLE_TENDENCIES_OK; i++) {
        size_t id_length = strlen(statistics[i].category) + strlen(statistics[i].label) + 16;
        size_t source_length = strlen(statistics[i].category) + strlen(corpus) + 2;
        char *id_text = (char *)malloc(id_length);
        char *source_text = (char *)malloc(source_length);
        EntityID *id = NULL;
        Value *source = NULL;
        if (id_text == NULL || source_text == NULL) {
            result = STYLE_TENDENCIES_ERR_ALLOC;
        } else {
            snprintf(id_text, id_length, "tendency:%s:%s", statistics[i].category, statistics[i].label);
            snprintf(source_text, source_length, "%s\t%s", statistics[i].category, corpus);
            if (entity_id_create(id_text, &id) != ENTITY_ID_OK ||
                value_create(STYLE_TENDENCY_SOURCE_VALUE_TYPE, source_text, strlen(source_text), &source) != VALUE_OK) {
                result = STYLE_TENDENCIES_ERR_ALLOC;
            } else if (tendency_create(id, statistics[i].label, statistics[i].count, statistics[i].total, source,
                                       &tendencies[i]) != TENDENCY_OK) {
                result = STYLE_TENDENCIES_ERR_MALFORMED;
            }
            if (result != STYLE_TENDENCIES_OK) {
                /* A Tendency that was not created owns neither the id nor the source. */
                entity_id_destroy(id);
                value_destroy(source);
            }
        }
        free(id_text);
        free(source_text);
    }
    style_statistics_free(statistics, count);

    if (result != STYLE_TENDENCIES_OK) {
        style_tendencies_free(tendencies, count);
        return result;
    }
    *out = tendencies;
    *out_count = count;
    return STYLE_TENDENCIES_OK;
}
