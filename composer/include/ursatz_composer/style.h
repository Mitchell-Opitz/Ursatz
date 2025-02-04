#ifndef URSATZ_COMPOSER_STYLE_H
#define URSATZ_COMPOSER_STYLE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/style_profile.h>

/*
 * A style the Composer prefers (R-080): a StyleModel file the Analyzer wrote from a corpus (style_model_file_write),
 * read through the Analyzer's own reader and kept as, per category, how often each label was seen. A style is a
 * count, not a rule (Laws 12 and 13): it is used to prefer one candidate over another, never to reject one.
 */

typedef struct ComposerStyle ComposerStyle;

typedef enum {
    COMPOSER_STYLE_OK = 0,
    COMPOSER_STYLE_ERR_ARGUMENT,
    COMPOSER_STYLE_ERR_IO,     /* the file could not be read */
    COMPOSER_STYLE_ERR_FORMAT, /* not a style model file (style_model_file_read said so) */
    COMPOSER_STYLE_ERR_ALLOC
} ComposerStyleResult;

/* A fraction, not reduced unless a function says so. denominator is at least 1 where a function gives one. */
typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} ComposerRatio;

const char *composer_style_result_name(ComposerStyleResult result);

/* Reads the model file at path into *out, which the caller destroys. *out is NULL on failure. */
ComposerStyleResult composer_style_load(const char *path, ComposerStyle **out);

/* NULL is a no-op. */
void composer_style_destroy(ComposerStyle *style);

/* The model the file was read into: for comparing a fragment with the style (analyzer_fragment_style_distance). */
const StyleModel *composer_style_model(const ComposerStyle *style);

/* How many pieces the style was counted from. */
size_t composer_style_pieces(const ComposerStyle *style);

/*
 * What the style holds for one label of one category: how often it was seen (count), how many things the category
 * counted (total), and the count of the category's most frequent label (largest). The label's share is count/total
 * and the category's largest share is largest/total, exactly. A label the category never saw has count 0. False
 * when the style has no such category (a style from a corpus that gave it nothing to count).
 */
typedef struct {
    unsigned long count;
    unsigned long total;
    unsigned long largest;
} ComposerStyleShare;

bool composer_style_share(const ComposerStyle *style, const char *category, const char *label, ComposerStyleShare *out);

#endif
