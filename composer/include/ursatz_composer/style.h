#ifndef URSATZ_COMPOSER_STYLE_H
#define URSATZ_COMPOSER_STYLE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/style_profile.h>

#include <ursatz_composer/piece_plan.h>

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

/*
 * The labels of a piece of material, in the Analyzer's words (style_labels.h): the Composer's candidates are counted
 * the way the Analyzer counts a corpus, so a candidate's label and a style's label meet under the same name.
 */
typedef struct ComposerStyleLabels ComposerStyleLabels;

/* NULL on allocation failure. NULL is a no-op to destroy. */
ComposerStyleLabels *composer_style_labels_create(void);
void composer_style_labels_destroy(ComposerStyleLabels *labels);

size_t composer_style_labels_count(const ComposerStyleLabels *labels);
/* Label i, in the order added. The strings are valid until labels is destroyed. False for i out of range. */
bool composer_style_labels_at(const ComposerStyleLabels *labels, size_t i, const char **category, const char **label);
/* Adds one label (copied). False on allocation failure. */
bool composer_style_labels_add(ComposerStyleLabels *labels, const char *category, const char *label);

/*
 * The harmony labels of a run of chords written as Roman numerals ("V6/5", "I", "V7/V"; a numeral that is not one is
 * skipped): `chord-numeral` for each, and `chord-progression-2` and `-3` for each step to a numeral that differs from the
 * one before it (a numeral that repeats is a chord but not a step). previous, when not NULL, is the chord written
 * before the first one: the first element of the first pair. The triples start at the first numeral given, so with one
 * chord before the first there is no triple before the third chord.
 */
bool composer_style_labels_add_harmony(ComposerStyleLabels *labels, const char *previous, const char *const *numerals,
                                       size_t count);

/* One melody note: its MIDI pitch and its length as a fraction of a whole note. */
typedef struct {
    int pitch;
    unsigned long duration_numerator;
    unsigned long duration_denominator;
} ComposerStyleNote;

/*
 * The melody labels of a run of notes: `duration` for each note, and `melodic-interval` and `contour` for each step
 * from one note to the next. before, when not NULL, is the pitch of the note written before the first one.
 */
bool composer_style_labels_add_melody(ComposerStyleLabels *labels, const int *before, const ComposerStyleNote *notes,
                                      size_t count);

/*
 * How close material is to the style, in [0, 1]: the mean over its labels of the label's share in its category divided
 * by the category's largest share (count over the largest count, exact), so the style's most common label scores 1
 * and a label it never saw 0. A label whose category the style does not have says nothing and is left out; material
 * with no labels is neutral, 1/1. The fraction is reduced; one that would not fit in 64 bits is rounded to 30 bits.
 */
bool composer_style_closeness(const ComposerStyle *style, const ComposerStyleLabels *labels, ComposerRatio *out);

/*
 * How far a plan is from the style, measured by the Analyzer and nothing the Composer scores with: every note the plan
 * wrote is rendered (plan_render), analyzed in memory (analyzer_analyze_fragment) in the key the plan starts in, and
 * the result compared with the style (analyzer_fragment_style_distance). *out is released with
 * style_comparison_destroy. The categories the plan has no data for stay in out->categories with in_a false and are left
 * out of the means.
 */
typedef enum {
    COMPOSER_MEASURE_OK = 0,
    COMPOSER_MEASURE_ERR_ARGUMENT,
    COMPOSER_MEASURE_ERR_ALLOC,
    COMPOSER_MEASURE_ERR_NOT_WRITTEN, /* the plan renders no notes */
    COMPOSER_MEASURE_ERR_PLAN,        /* no key at the start, or the plan cannot be rendered */
    COMPOSER_MEASURE_ERR_ANALYSIS     /* the Analyzer could not analyze or compare it */
} ComposerMeasureResult;

ComposerMeasureResult composer_style_measure(const PiecePlan *plan, const ComposerStyle *style, StyleComparison *out);

#endif
