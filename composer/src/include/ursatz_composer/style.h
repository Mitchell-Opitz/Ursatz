#ifndef URSATZ_COMPOSER_STYLE_H
#define URSATZ_COMPOSER_STYLE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/style_labels.h>
#include <ursatz_analyzer/style_profile.h>

#include <ursatz_composer/motive.h>
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
    COMPOSER_STYLE_ERR_FORMAT, /* not a style model file (style_model_file_read_without said so) */
    COMPOSER_STYLE_ERR_ALLOC
} ComposerStyleResult;

/* A fraction, not reduced unless a function says so. denominator is at least 1 where a function gives one. */
typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} ComposerRatio;

const char *composer_style_result_name(ComposerStyleResult result);

/* Reads the model file at path into *out, which the caller destroys. *out is NULL on failure. Every category is kept. */
ComposerStyleResult composer_style_load(const char *path, ComposerStyle **out);

/*
 * composer_style_load without the named categories (style_model_file_read_without): the style holds no statistics of
 * them, and keeps the list. A category that is not one the Analyzer defines is COMPOSER_STYLE_ERR_ARGUMENT.
 * composer_style_load is this with none.
 */
ComposerStyleResult composer_style_load_without(const char *path, const char *const *categories, size_t count,
                                                ComposerStyle **out);

/* The categories the style was read without, in the order given (a blend has both lists, repeats once). */
size_t composer_style_without_count(const ComposerStyle *style);
/* Entry i, or NULL when i is out of range. Valid until the style is destroyed. */
const char *composer_style_without_at(const ComposerStyle *style, size_t i);

/* NULL is a no-op. */
void composer_style_destroy(ComposerStyle *style);

/* The model the file was read into: for comparing a result with the style (analyzer_style_reference_create). */
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
 * What the style prefers for a brief (R-163). composer_style_tempo: the middle of the most common `tempo` band in
 * quarter notes per minute (ties: the lower band; `140-159` gives 150, `<40` gives 30, `220+` gives 230), a whole
 * number over 1. composer_style_mode: the most common `mode` (ties: major). composer_style_density: the most common
 * `note-density` as attacks per beat (`4+` reads 4), halves over 2, not reduced (ties: the lower density). Each is
 * false, leaving *out alone, when the style has no such category or no label it can read.
 */
bool composer_style_tempo(const ComposerStyle *style, ComposerRatio *out);
bool composer_style_mode(const ComposerStyle *style, bool *minor);
bool composer_style_density(const ComposerStyle *style, ComposerRatio *out);

/*
 * A style built from a model in memory, for a blend (composer_style_blend). Takes ownership of model on success only,
 * and reads its statistics the way composer_style_load does; every category of the model must give each of its labels
 * the same total, as the Analyzer's files and composer_style_blend's results do. pieces is what composer_style_pieces
 * returns.
 */
ComposerStyleResult composer_style_from_model(StyleModel *model, size_t pieces, ComposerStyle **out);

/*
 * The weight of the second style of a blend in each dimension (StyleDimension: harmony, rhythm, melody, form), as a
 * fraction from 0 (only the first) to 1 (only the second). A denominator of 0 is invalid.
 */
typedef struct {
    unsigned long numerator[STYLE_DIMENSION_COUNT];
    unsigned long denominator[STYLE_DIMENSION_COUNT];
} ComposerStyleWeights;

/* The same weight, weight, in every dimension. */
ComposerStyleWeights composer_style_weights_all(ComposerRatio weight);

/*
 * Blends two styles with a weight per dimension. For each dimension the statistics of that dimension's categories
 * (style_category_dimension) of each style become a model (style_model_from_statistics), the two are blended at that
 * dimension's weight (style_model_blend: frequency = (1 - w) * a + w * b, exact), and the four results are merged.
 * Each category is then brought to one total (its labels' frequencies over their common denominator, or over 2^30
 * when that is too large), so the result reads like a model the Analyzer wrote. A weight of 0 gives a's frequencies
 * in a dimension and 1 gives b's; a category only one style has is blended with nothing. The result is named
 * "<a> + <b>", its training corpus is both joined with ';', and its pieces are the sum. COMPOSER_STYLE_ERR_ARGUMENT
 * for a weight above 1 or a zero denominator. *out is destroyed by the caller.
 */
ComposerStyleResult composer_style_blend(const ComposerStyle *a, const ComposerStyle *b,
                                         const ComposerStyleWeights *weights, ComposerStyle **out);

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

/*
 * The form labels of one theme: `theme-form` is form (the name the Analyzer gives a theme: "sentence", "period",
 * "repeated-phrase", "compound-period", "hybrid", "unique") and `phrase-length` is each phrase's length in bars.
 */
bool composer_style_labels_add_form(ComposerStyleLabels *labels, const char *form, const unsigned *phrase_bars,
                                    size_t phrase_count);

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
 * The motive labels of a full motive (style_labels.h, the Analyzer's words): `motive-degrees` (each note's pitch class
 * as a degree of the tonic, joined by spaces), `motive-intervals` (the signed semitone steps, none for a single note) and
 * `motive-rhythm` (each note's duration, joined). A rhythm or contour motive has no pitches and adds nothing.
 */
bool composer_style_labels_add_motive(ComposerStyleLabels *labels, const Motive *motive, int tonic_pc);

/*
 * One `motive-nct-figure` label for a non-chord tone: the degree before it, its own degree and its type, and the degree
 * after it (previous_degree and next_degree may be NULL for none, written "-"). type is the Analyzer document's name:
 * "passing", "neighbor", "appoggiatura", "suspension" or "anticipation".
 */
bool composer_style_labels_add_nct(ComposerStyleLabels *labels, const char *previous_degree, const char *degree,
                                   const char *type, const char *next_degree);

/*
 * How close material is to the style, in [0, 1]: the mean over its labels of the label's share in its category divided
 * by the category's largest share (count over the largest count, exact), so the style's most common label scores 1
 * and a label it never saw 0. A label whose category the style does not have says nothing and is left out; material
 * with no labels is neutral, 1/1. The fraction is reduced; one that would not fit in 64 bits is rounded to 30 bits.
 */
bool composer_style_closeness(const ComposerStyle *style, const ComposerStyleLabels *labels, ComposerRatio *out);

/*
 * How far a plan is from the style, measured by the Analyzer and nothing the Composer scores with: every note the plan
 * wrote is rendered (plan_render), heard (analyzer_hear) in the key the plan starts in, and the result compared with
 * the style (analyzer_result_style_distance, against the AnalyzerStyleReference the style keeps). *out is released with
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
