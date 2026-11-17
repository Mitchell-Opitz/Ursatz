#ifndef MUSIC_THEORY_STYLE_MODEL_BUILD_H
#define MUSIC_THEORY_STYLE_MODEL_BUILD_H

#include <stddef.h>

#include "model/identity/entity_id.h"
#include "theory/style_features.h"
#include "theory/style_model.h"

/*
 * Builds a StyleModel from corpus statistics.
 *
 * Counting is not fitting: a StyleModel built here holds what a corpus's
 * persisted interpretations say, as exact-rational frequencies, and nothing
 * more. Each statistic is observed behavior that never implies
 * must/must-not, and a StyleModel offers no way to become a Rule or a
 * Constraint: theory/ includes nothing from rules/, and no function here
 * accepts or returns one. It cannot be promoted.
 *
 * The statistics arrive as plain values (StyleStatistic), the opaque-value
 * boundary theory/ has with the corpus: this module has no dependency on
 * any storage or corpus module (Dependency Spec §7.2).
 */

typedef enum {
    STYLE_MODEL_BUILD_OK = 0,
    STYLE_MODEL_BUILD_ERR_NULL_ARGUMENT,
    STYLE_MODEL_BUILD_ERR_INVALID_STATISTIC, /* see StyleStatistic */
    STYLE_MODEL_BUILD_ERR_DUPLICATE,         /* the same (category, label) twice */
    STYLE_MODEL_BUILD_ERR_MODEL,             /* style_model_create rejected id, name, corpus label or version */
    STYLE_MODEL_BUILD_ERR_ALLOC
} StyleModelBuildResult;

/*
 * Builds a StyleModel whose style_features hold the statistics
 * (style_features_encode) and whose training-corpus label and version are the
 * ones given. Takes ownership of id on success only (as style_model_create
 * does); the statistics are copied. An empty list (count 0) is valid.
 */
StyleModelBuildResult style_model_from_statistics(EntityID *id, const char *style_name, const char *training_corpus,
                                                  const char *model_version, const StyleStatistic *statistics,
                                                  size_t count, StyleModel **out);

#endif
