#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "theory/statistical_model.h"

/*
 * Construction/shape test: a StatisticalModel holds identity,
 * input_features, output, probability fraction, training_corpus
 * (opaque label, not a music-corpus reference), model_version, and
 * optional calibration -- all round-trip unchanged. No corpus
 * ingestion/regression test here (see file header / OPEN ISSUE); there
 * is no Corpus module yet to regress against.
 */

int main(void) {
    Value *features;
    assert(value_create("input-features", "interval-histogram", 19, &features) == VALUE_OK);
    Value *output;
    assert(value_create("output", "cadence-likelihood", 19, &output) == VALUE_OK);
    Value *calibration;
    assert(value_create("calibration", "platt-scaling-v1", 16, &calibration) == VALUE_OK);

    EntityID *id;
    assert(entity_id_create("statistical-model-cadence-predictor", &id) == ENTITY_ID_OK);

    StatisticalModel *model;
    assert(statistical_model_create(id, features, output, 7, 10, "bach-chorales-v1", "v0.1.0", calibration,
                                     &model) == STATISTICAL_MODEL_OK);
    assert(statistical_model_input_features(model) == features);
    assert(statistical_model_output(model) == output);
    assert(statistical_model_probability_numerator(model) == 7);
    assert(statistical_model_probability_denominator(model) == 10);
    assert(strcmp(statistical_model_training_corpus(model), "bach-chorales-v1") == 0);
    assert(strcmp(statistical_model_model_version(model), "v0.1.0") == 0);
    assert(statistical_model_calibration(model) == calibration);

    EntityID *other_id;
    assert(entity_id_create("statistical-model-other", &other_id) == ENTITY_ID_OK);
    Value *other_features;
    assert(value_create("input-features", "x", 1, &other_features) == VALUE_OK);
    Value *other_output;
    assert(value_create("output", "y", 1, &other_output) == VALUE_OK);
    StatisticalModel *other;
    assert(statistical_model_create(other_id, other_features, other_output, 1, 2, "corpus-b", "v0.1.0", NULL,
                                     &other) == STATISTICAL_MODEL_OK);

    assert(statistical_model_identity_equals(model, model));
    assert(!statistical_model_identity_equals(model, other));

    statistical_model_destroy(model);
    statistical_model_destroy(other);

    printf("test_statistical_model: ok\n");
    return 0;
}