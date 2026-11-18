#ifndef URSATZ_THEORY_STYLE_MODEL_FILE_H
#define URSATZ_THEORY_STYLE_MODEL_FILE_H

#include <stddef.h>

#include "theory/style/style_features.h"
#include "theory/style/style_model.h"

/*
 * The StyleModel file (schema/style-model.v1.schema.json): a style's statistics with its name, the training corpus, the
 * version of what produced it and the piece count. Reading is the library's (R-617); the Analyzer writes the file from a
 * profile (style_model_file_write in the Analyzer API) and the Composer reads it through style_model_file_read_without.
 */

#define STYLE_MODEL_FILE_SCHEMA "ursatz-style-model"
#define STYLE_MODEL_FILE_SCHEMA_VERSION 1

typedef enum {
    STYLE_MODEL_FILE_OK = 0,
    STYLE_MODEL_FILE_ERR_ARGUMENT,
    STYLE_MODEL_FILE_ERR_IO,
    STYLE_MODEL_FILE_ERR_FORMAT, /* not a style model file, an unknown dimension or category, or inconsistent counts */
    STYLE_MODEL_FILE_ERR_ALLOC
} StyleModelFileResult;

/*
 * Reads a style model file into a StyleModel (style_model_from_statistics) the caller destroys with
 * style_model_destroy. Its id is "style-model-" and the 16 hex digits of the FNV-1a 64 hash of the file's bytes, so
 * the same file always gives the same id. pieces may be NULL.
 */
StyleModelFileResult style_model_file_read(const char *path, StyleModel **out, size_t *pieces);

/*
 * style_model_file_read without the named categories: they are dropped before the model is built, so it holds no
 * statistics of them. An unknown category name is STYLE_MODEL_FILE_ERR_ARGUMENT. style_model_file_read is this with none.
 * The Composer reads styles without the motive categories until it uses them as preferences.
 */
StyleModelFileResult style_model_file_read_without(const char *path, const char *const *categories, size_t count,
                                                   StyleModel **out, size_t *pieces);

/* The statistics a model holds (style_features_decode); free with style_statistics_free. */
StyleModelFileResult style_model_statistics(const StyleModel *model, StyleStatistic **out, size_t *count);

#endif
