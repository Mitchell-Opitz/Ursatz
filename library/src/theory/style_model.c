#include "theory/style_model.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct StyleModel {
    EntityID *id;
    char *style_name;
    Value *style_features;
    char *training_corpus;
    char *model_version;
};

StyleModelResult style_model_create(EntityID *id, const char *style_name, Value *style_features,
                                     const char *training_corpus, const char *model_version, StyleModel **out) {
    if (id == NULL) {
        return STYLE_MODEL_ERR_NULL_ID;
    }
    if (style_name == NULL || style_name[0] == '\0') {
        return STYLE_MODEL_ERR_NULL_STYLE_NAME;
    }
    if (style_features == NULL) {
        return STYLE_MODEL_ERR_NULL_STYLE_FEATURES;
    }
    if (training_corpus == NULL || training_corpus[0] == '\0') {
        return STYLE_MODEL_ERR_NULL_TRAINING_CORPUS;
    }
    if (model_version == NULL || model_version[0] == '\0') {
        return STYLE_MODEL_ERR_NULL_MODEL_VERSION;
    }

    StyleModel *model = (StyleModel *)malloc(sizeof(StyleModel));
    if (model == NULL) {
        return STYLE_MODEL_ERR_ALLOC;
    }

    model->style_name = ursatz_string_copy(style_name);
    if (model->style_name == NULL) {
        free(model);
        return STYLE_MODEL_ERR_ALLOC;
    }

    model->training_corpus = ursatz_string_copy(training_corpus);
    if (model->training_corpus == NULL) {
        free(model->style_name);
        free(model);
        return STYLE_MODEL_ERR_ALLOC;
    }

    model->model_version = ursatz_string_copy(model_version);
    if (model->model_version == NULL) {
        free(model->style_name);
        free(model->training_corpus);
        free(model);
        return STYLE_MODEL_ERR_ALLOC;
    }

    model->id = id;
    model->style_features = style_features;

    *out = model;
    return STYLE_MODEL_OK;
}

void style_model_destroy(StyleModel *model) {
    if (model == NULL) {
        return;
    }
    entity_id_destroy(model->id);
    free(model->style_name);
    value_destroy(model->style_features);
    free(model->training_corpus);
    free(model->model_version);
    free(model);
}

const EntityID *style_model_id(const StyleModel *model) {
    return model->id;
}

const char *style_model_style_name(const StyleModel *model) {
    return model->style_name;
}

const Value *style_model_style_features(const StyleModel *model) {
    return model->style_features;
}

const char *style_model_training_corpus(const StyleModel *model) {
    return model->training_corpus;
}

const char *style_model_model_version(const StyleModel *model) {
    return model->model_version;
}

bool style_model_identity_equals(const StyleModel *a, const StyleModel *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}