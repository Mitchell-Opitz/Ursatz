#ifndef URSATZ_ANALYZER_STYLE_PROFILE_H
#define URSATZ_ANALYZER_STYLE_PROFILE_H

#include <stdbool.h>
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

/*
 * style_model_file_read without the named categories: they are dropped before the model is built, so it holds no
 * statistics of them. An unknown category name is STYLE_MODEL_FILE_ERR_ARGUMENT. style_model_file_read is this with none.
 * The Composer reads styles without the motive categories until it uses them as preferences.
 */
StyleModelFileResult style_model_file_read_without(const char *path, const char *const *categories, size_t count,
                                                   StyleModel **out, size_t *pieces);

/* The statistics a model holds (style_features_decode); free with style_statistics_free. */
StyleModelFileResult style_model_statistics(const StyleModel *model, StyleStatistic **out, size_t *count);

/*
 * Comparison of two models. Per category the distance is the total variation distance between the two models'
 * label frequencies: half the sum over labels of |count_a/total_a - count_b/total_b|, an exact reduced rational in
 * [0, 1]. A category only one model has is distance 1. A dimension's distance is the mean of its categories'; the
 * overall distance is the mean of the dimensions that have a category in either model.
 */

/* An exact fraction in lowest terms. The fields are 128 bits wide: the mean of a dimension's categories has a
   denominator that is the least common multiple of theirs, which does not fit in 64 bits for real corpora. */
__extension__ typedef unsigned __int128 StyleRationalInt;

typedef struct {
    StyleRationalInt numerator;
    StyleRationalInt denominator;
} StyleRational;

/* The fraction as a double. */
double style_rational_value(StyleRational value);

/* "n/d" in decimal digits, written to at most size bytes, always terminated. */
void style_rational_format(StyleRational value, char *buffer, size_t size);

typedef struct {
    char *category;
    StyleDimension dimension;
    StyleRational distance;
    bool in_a, in_b;  /* which models have the category */
    char *top_a[3];   /* each model's most frequent labels, most frequent first; NULL after the last */
    char *top_b[3];
} StyleCategoryDistance;

typedef struct {
    StyleCategoryDistance *categories; /* sorted by category */
    size_t category_count;
    bool dimension_present[STYLE_DIMENSION_COUNT];
    StyleRational dimension[STYLE_DIMENSION_COUNT];
    bool overall_present;
    StyleRational overall;
} StyleComparison;

typedef enum {
    STYLE_COMPARE_OK = 0,
    STYLE_COMPARE_ERR_ARGUMENT,
    STYLE_COMPARE_ERR_MODEL,    /* a model's statistics cannot be read or name a category this module does not define */
    STYLE_COMPARE_ERR_OVERFLOW, /* an exact result does not fit; never wrapped */
    STYLE_COMPARE_ERR_ALLOC
} StyleCompareResult;

StyleCompareResult style_profile_compare(const StyleModel *a, const StyleModel *b, StyleComparison *out);
void style_comparison_destroy(StyleComparison *comparison);

#endif
