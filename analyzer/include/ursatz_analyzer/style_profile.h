#ifndef URSATZ_ANALYZER_STYLE_PROFILE_H
#define URSATZ_ANALYZER_STYLE_PROFILE_H

#include <stddef.h>

#include "theory/style_features.h"
#include "theory/style_model.h"

#include <ursatz_analyzer/style_labels.h>

/*
 * A style profile: counts of harmony, rhythm, melody and form labels (style_labels.h) over the analysis documents
 * of a corpus. It reads only the documents, never the scores (R-079). A profile is a count, not a rule: it says
 * how often something was seen.
 */

typedef struct StyleProfile StyleProfile;

typedef enum {
    STYLE_PROFILE_OK = 0,
    STYLE_PROFILE_ERR_ARGUMENT,
    STYLE_PROFILE_ERR_IO,      /* a file could not be read */
    STYLE_PROFILE_ERR_FORMAT,  /* not JSON, not an analysis document of schema version 9, or not a corpus index */
    STYLE_PROFILE_ERR_ALLOC
} StyleProfileResult;

/* NULL on allocation failure. */
StyleProfile *style_profile_create(void);
void style_profile_destroy(StyleProfile *profile);

/* Counts one analysis document (the text of an "analysis-document" with schema_version 9). */
StyleProfileResult style_profile_add_document_json(StyleProfile *profile, const char *json);

/*
 * Reads "<output_dir>/corpus-index.json" and adds every piece the index lists as analyzed, through the index's
 * document paths. The index's input_dir becomes the profile's training corpus (several corpora join with ';').
 */
StyleProfileResult style_profile_add_corpus(StyleProfile *profile, const char *output_dir);

/* How many documents were added. */
size_t style_profile_piece_count(const StyleProfile *profile);

/* The training corpus label, "" when no corpus was added. Valid until the profile is destroyed. */
const char *style_profile_training_corpus(const StyleProfile *profile);

/*
 * Every (category, label) seen, sorted by category then label (byte order), each with its count and the sum of
 * the counts of its category. *out is a new array the caller frees with free(); its strings belong to the profile
 * and stay valid until it is destroyed. *out is NULL when *count is 0.
 */
StyleProfileResult style_profile_statistics(const StyleProfile *profile, StyleStatistic **out, size_t *count);

/*
 * The StyleModel file (schema/style-model.v1.schema.json): the profile's statistics with the style's name, the
 * training corpus, the analyzer version and the piece count. The Composer reads it through
 * style_model_file_read.
 */

typedef enum {
    STYLE_MODEL_FILE_OK = 0,
    STYLE_MODEL_FILE_ERR_ARGUMENT,
    STYLE_MODEL_FILE_ERR_IO,
    STYLE_MODEL_FILE_ERR_FORMAT, /* not a style model file, an unknown dimension or category, or inconsistent counts */
    STYLE_MODEL_FILE_ERR_ALLOC
} StyleModelFileResult;

/* Writes the profile as a style model file named name. The same profile always gives the same bytes. */
StyleModelFileResult style_model_file_write(const StyleProfile *profile, const char *name, const char *path);

/*
 * Reads a style model file into a StyleModel (style_model_from_statistics) the caller destroys with
 * style_model_destroy. Its id is "style-model-" and the 16 hex digits of the FNV-1a 64 hash of the file's bytes, so
 * the same file always gives the same id. pieces may be NULL.
 */
StyleModelFileResult style_model_file_read(const char *path, StyleModel **out, size_t *pieces);

/* The statistics a model holds (style_features_decode); free with style_statistics_free. */
StyleModelFileResult style_model_statistics(const StyleModel *model, StyleStatistic **out, size_t *count);

#endif
