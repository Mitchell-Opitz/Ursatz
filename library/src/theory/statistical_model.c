#include "theory/statistical_model.h"

#include <stdlib.h>
#include <string.h>

struct StatisticalModel {
    EntityID *id;
    Value *input_features;
    Value *output;
    unsigned long probability_numerator;
    unsigned long probability_denominator;
    char *training_corpus;
    char *model_version;
    Value *calibration;
};

static char *copy_string(const char *value) {
    size_t len = strlen(value);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, value, len + 1);
    }
    return copy;
}

StatisticalModelResult statistical_model_create(EntityID *id, Value *input_features, Value *output,
                                                  unsigned long probability_numerator,
                                                  unsigned long probability_denominator, const char *training_corpus,
                                                  const char *model_version, Value *calibration,
                                                  StatisticalModel **out) {
    if (id == NULL) {
        return STATISTICAL_MODEL_ERR_NULL_ID;
    }
    if (input_features == NULL) {
        return STATISTICAL_MODEL_ERR_NULL_INPUT_FEATURES;
    }
    if (output == NULL) {
        return STATISTICAL_MODEL_ERR_NULL_OUTPUT;
    }
    if (training_corpus == NULL || training_corpus[0] == '\0') {
        return STATISTICAL_MODEL_ERR_NULL_TRAINING_CORPUS;
    }
    if (model_version == NULL || model_version[0] == '\0') {
        return STATISTICAL_MODEL_ERR_NULL_MODEL_VERSION;
    }
    if (probability_denominator == 0) {
        return STATISTICAL_MODEL_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (probability_numerator > probability_denominator) {
        return STATISTICAL_MODEL_ERR_PROBABILITY_OUT_OF_RANGE;
    }

    StatisticalModel *model = (StatisticalModel *)malloc(sizeof(StatisticalModel));
    if (model == NULL) {
        return STATISTICAL_MODEL_ERR_ALLOC;
    }

    model->training_corpus = copy_string(training_corpus);
    if (model->training_corpus == NULL) {
        free(model);
        return STATISTICAL_MODEL_ERR_ALLOC;
    }

    model->model_version = copy_string(model_version);
    if (model->model_version == NULL) {
        free(model->training_corpus);
        free(model);
        return STATISTICAL_MODEL_ERR_ALLOC;
    }

    model->id = id;
    model->input_features = input_features;
    model->output = output;
    model->probability_numerator = probability_numerator;
    model->probability_denominator = probability_denominator;
    model->calibration = calibration;

    *out = model;
    return STATISTICAL_MODEL_OK;
}

void statistical_model_destroy(StatisticalModel *model) {
    if (model == NULL) {
        return;
    }
    entity_id_destroy(model->id);
    value_destroy(model->input_features);
    value_destroy(model->output);
    free(model->training_corpus);
    free(model->model_version);
    value_destroy(model->calibration);
    free(model);
}

const EntityID *statistical_model_id(const StatisticalModel *model) {
    return model->id;
}

const Value *statistical_model_input_features(const StatisticalModel *model) {
    return model->input_features;
}

const Value *statistical_model_output(const StatisticalModel *model) {
    return model->output;
}

unsigned long statistical_model_probability_numerator(const StatisticalModel *model) {
    return model->probability_numerator;
}

unsigned long statistical_model_probability_denominator(const StatisticalModel *model) {
    return model->probability_denominator;
}

const char *statistical_model_training_corpus(const StatisticalModel *model) {
    return model->training_corpus;
}

const char *statistical_model_model_version(const StatisticalModel *model) {
    return model->model_version;
}

const Value *statistical_model_calibration(const StatisticalModel *model) {
    return model->calibration;
}

bool statistical_model_identity_equals(const StatisticalModel *a, const StatisticalModel *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}