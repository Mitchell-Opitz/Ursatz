#ifndef MUSIC_THEORY_STYLE_FEATURES_H
#define MUSIC_THEORY_STYLE_FEATURES_H

#include <stddef.h>

#include "core/value.h"

/*
 * The content of a StyleModel's style_features Value: a list of corpus
 * statistics, each the count of one label within one category and that
 * category's total (so the label's frequency is count/total, an exact
 * rational). StyleModel keeps style_features as an opaque Value
 * (theory/style_model.h); this is the one format this module gives it, so a
 * StyleModel built from corpus statistics and one read back agree.
 *
 * Statistics describe what a corpus's already-persisted interpretations say.
 * They are non-authoritative: a Tendency built from one (theory/tendency.h)
 * states how often something was seen, never that it is required or
 * forbidden, and nothing in this module produces or accepts a Rule or
 * Constraint.
 *
 * Value type STYLE_FEATURES_VALUE_TYPE; content is text, one statistic per
 * line, "category<TAB>label<TAB>count<TAB>total<NEWLINE>", sorted by category
 * and then label (byte order), at most one line per (category, label). The
 * counts are decimal integers; there is no floating point. Categories and
 * labels are non-empty and contain neither a tab nor a newline.
 */
#define STYLE_FEATURES_VALUE_TYPE "style-tendencies-v1"

typedef struct {
    const char *category; /* e.g. "chord-progression-2", "cadence-type" */
    const char *label;
    unsigned long count;  /* how many times label was seen in category */
    unsigned long total;  /* how many things category counted; count <= total, total > 0 */
} StyleStatistic;

typedef enum {
    STYLE_FEATURES_OK = 0,
    STYLE_FEATURES_ERR_NULL_ARGUMENT,
    STYLE_FEATURES_ERR_INVALID_STATISTIC, /* empty or tab/newline-bearing name, total 0, or count above total */
    STYLE_FEATURES_ERR_DUPLICATE,         /* the same (category, label) twice */
    STYLE_FEATURES_ERR_MALFORMED,         /* decode: not a style-tendencies-v1 Value */
    STYLE_FEATURES_ERR_ALLOC
} StyleFeaturesResult;

/*
 * Packs the statistics into a new Value the caller owns (count may be 0,
 * giving an empty list). Input order does not matter; the content is sorted.
 */
StyleFeaturesResult style_features_encode(const StyleStatistic *statistics, size_t count, Value **out);

/*
 * Reads a Value made by style_features_encode. On success *out is a newly
 * allocated array of *out_count statistics (NULL when 0) whose strings the
 * array owns; free it with style_statistics_free. A Value of another type, or
 * content that is not well formed, sorted and duplicate-free, is MALFORMED.
 */
StyleFeaturesResult style_features_decode(const Value *features, StyleStatistic **out, size_t *out_count);

/* Frees a list returned by style_features_decode. Safe on NULL. */
void style_statistics_free(StyleStatistic *statistics, size_t count);

#endif
