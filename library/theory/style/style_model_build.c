#include "theory/style/style_model_build.h"

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
