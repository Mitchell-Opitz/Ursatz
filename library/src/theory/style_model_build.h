#ifndef MUSIC_THEORY_STYLE_MODEL_BUILD_H
#define MUSIC_THEORY_STYLE_MODEL_BUILD_H

#include <stddef.h>

#include "identity/entity_id.h"
#include "theory/style_features.h"
#include "theory/style_model.h"
#include "theory/tendency.h"

/*
 * Builds a StyleModel from corpus statistics, and reads its statistics back
 * as Tendencies.
 *
 * Counting is not fitting: a StyleModel built here holds what a corpus's
 * persisted interpretations say, as exact-rational frequencies, and nothing
 * more. Each statistic is a Tendency (theory/tendency.h) -- observed
 * behavior that never implies must/must-not -- and a StyleModel offers no
 * way to become a Rule or a Constraint: theory/ includes nothing from rules/,
 * and no function here accepts or returns one. It cannot be promoted.
 *
 * The statistics arrive as plain values (StyleStatistic), the opaque-value
 * boundary theory/ has with the corpus: this module has no dependency on
 * corpus_stats/ or corpus/ (Dependency Spec §7.2).
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

typedef enum {
    STYLE_TENDENCIES_OK = 0,
    STYLE_TENDENCIES_ERR_NULL_ARGUMENT,
    STYLE_TENDENCIES_ERR_MALFORMED, /* the model's style_features are not a style-tendencies-v1 Value */
    STYLE_TENDENCIES_ERR_ALLOC
} StyleTendenciesResult;

/*
 * Each statistic of the model as a new Tendency the caller owns, in the
 * statistics' sorted order (category, then label). A Tendency's label is the
 * statistic's label and its frequency is count/total, unreduced as stored;
 * its observed_data is a Value of type STYLE_TENDENCY_SOURCE_VALUE_TYPE whose
 * content is "<category><TAB><training corpus label>", so the category and
 * the training corpus the count came from travel with it. *out is NULL when
 * *out_count is 0. Free with style_tendencies_free.
 */
#define STYLE_TENDENCY_SOURCE_VALUE_TYPE "style-statistic-source"

StyleTendenciesResult style_model_tendencies(const StyleModel *model, Tendency ***out, size_t *out_count);

/* Destroys the Tendencies and the array. Safe on NULL. */
void style_tendencies_free(Tendency **tendencies, size_t count);

#endif
