#ifndef MUSIC_THEORY_STYLE_MODEL_H
#define MUSIC_THEORY_STYLE_MODEL_H

#include <stdbool.h>

#include "core/value.h"
#include "identity/entity_id.h"

/*
 * StyleModel (Category / base type): Registry groups this with
 * Tendency (theory/tendency.h) and StatisticalModel
 * (theory/statistical_model.h) under the same Theory -> Corpus open
 * issue (Dependency Spec §7.2) -- build shape only.
 *
 * Fields: id, style_name, style_features, training_corpus,
 * model_version.
 *
 * style_features is an opaque Value (core/value.h): this type does not
 * define what a captured style signature looks like, only a labeled
 * container for whatever an L8/L9-adjacent producer supplies.
 *
 * training_corpus is an opaque identifier/label string only -- never a
 * music-corpus object reference, same treatment StatisticalModel gives
 * its own training_corpus field. This module has no include of any
 * music-corpus header.
 *
 * Construction/shape only in this pass: no ingestion, fitting, or
 * comparison logic -- that is L8/L9-adjacent and out of scope here.
 */

typedef struct StyleModel StyleModel;

typedef enum {
    STYLE_MODEL_OK = 0,
    STYLE_MODEL_ERR_NULL_ID,
    STYLE_MODEL_ERR_NULL_STYLE_NAME,
    STYLE_MODEL_ERR_NULL_STYLE_FEATURES,
    STYLE_MODEL_ERR_NULL_TRAINING_CORPUS,
    STYLE_MODEL_ERR_NULL_MODEL_VERSION,
    STYLE_MODEL_ERR_ALLOC
} StyleModelResult;

/*
 * Takes ownership of id and style_features on success -- both
 * destroyed by style_model_destroy. style_name, training_corpus, and
 * model_version are copied, required, non-empty.
 */
StyleModelResult style_model_create(EntityID *id, const char *style_name, Value *style_features,
                                     const char *training_corpus, const char *model_version, StyleModel **out);
void style_model_destroy(StyleModel *model);

const EntityID *style_model_id(const StyleModel *model);
const char *style_model_style_name(const StyleModel *model);
const Value *style_model_style_features(const StyleModel *model);
const char *style_model_training_corpus(const StyleModel *model);
const char *style_model_model_version(const StyleModel *model);

bool style_model_identity_equals(const StyleModel *a, const StyleModel *b);

#endif