#ifndef MUSIC_THEORY_STATISTICAL_MODEL_H
#define MUSIC_THEORY_STATISTICAL_MODEL_H

#include <stdbool.h>

#include "core/value.h"
#include "identity/entity_id.h"

/*
 * StatisticalModel (Category / base type): an explicit object (TDD
 * §100) wrapping a corpus-derived statistical model.
 *
 * Fields: id, input_features, output, probability, training_corpus,
 * model_version, calibration.
 *
 * input_features, output, and calibration are opaque Values
 * (core/value.h) -- this type does not define a feature-vector or
 * output schema, only a labeled container for whatever an L8 model
 * producer supplies. probability is an exact rational fraction
 * (numerator/denominator, Law 19, never float/double), the same
 * representation Evidence and Tendency use.
 *
 * training_corpus is an opaque identifier/label string only (per the
 * OPEN ISSUE, Dependency Spec §7.2) -- never a music-corpus object
 * reference. This module has no include of any music-corpus header.
 *
 * A StatisticalModel's output MUST NOT automatically become a theory
 * Rule: there is no promotion/conversion function anywhere in this
 * header or its implementation that produces a Rule (rules/rule.h)
 * from a StatisticalModel, and this module does not include
 * rules/rule.h at all.
 */

typedef struct StatisticalModel StatisticalModel;

typedef enum {
    STATISTICAL_MODEL_OK = 0,
    STATISTICAL_MODEL_ERR_NULL_ID,
    STATISTICAL_MODEL_ERR_NULL_INPUT_FEATURES,
    STATISTICAL_MODEL_ERR_NULL_OUTPUT,
    STATISTICAL_MODEL_ERR_NULL_TRAINING_CORPUS,
    STATISTICAL_MODEL_ERR_NULL_MODEL_VERSION,
    STATISTICAL_MODEL_ERR_NON_POSITIVE_DENOMINATOR,
    STATISTICAL_MODEL_ERR_PROBABILITY_OUT_OF_RANGE,
    STATISTICAL_MODEL_ERR_ALLOC
} StatisticalModelResult;

/*
 * Takes ownership of id, input_features, output (and of calibration,
 * if non-NULL) on success -- all destroyed by
 * statistical_model_destroy. training_corpus and model_version are
 * copied, required, non-empty. probability is a fraction in [0, 1]:
 * numerator <= denominator, denominator > 0. calibration is optional
 * -- pass NULL if not supplied.
 */
StatisticalModelResult statistical_model_create(EntityID *id, Value *input_features, Value *output,
                                                  unsigned long probability_numerator,
                                                  unsigned long probability_denominator, const char *training_corpus,
                                                  const char *model_version, Value *calibration,
                                                  StatisticalModel **out);
void statistical_model_destroy(StatisticalModel *model);

const EntityID *statistical_model_id(const StatisticalModel *model);
const Value *statistical_model_input_features(const StatisticalModel *model);
const Value *statistical_model_output(const StatisticalModel *model);
unsigned long statistical_model_probability_numerator(const StatisticalModel *model);
unsigned long statistical_model_probability_denominator(const StatisticalModel *model);
const char *statistical_model_training_corpus(const StatisticalModel *model);
const char *statistical_model_model_version(const StatisticalModel *model);
const Value *statistical_model_calibration(const StatisticalModel *model);

bool statistical_model_identity_equals(const StatisticalModel *a, const StatisticalModel *b);

#endif