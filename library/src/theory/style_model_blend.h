#ifndef MUSIC_THEORY_STYLE_MODEL_BLEND_H
#define MUSIC_THEORY_STYLE_MODEL_BLEND_H

#include "identity/entity_id.h"
#include "theory/style_model.h"

/*
 * Blends two StyleModels built from corpus statistics
 * (theory/style_model_build.h) with a weight: a new StyleModel whose
 * frequencies are the weighted combination
 *
 *     frequency = (1 - w) * frequency_a + w * frequency_b
 *
 * for every (category, label) in either model, where a label a model does not
 * have counts as frequency 0 there. w = weight_numerator / weight_denominator,
 * an exact rational in [0, 1] inclusive; everything is exact (no floating
 * point), reduced to lowest terms, and checked: an overflow is an error, never
 * a wrapped value.
 *
 * Weight 0 gives model A's frequencies and weight 1 model B's, as reduced
 * fractions over the same labels: a label whose blended frequency is 0 (one
 * only the other model has) is left out. In the result a statistic's count and
 * total are the numerator and denominator of the blended frequency, so count
 * is no longer a literal tally; the result is a valid input to a further
 * blend. The blend is as non-authoritative as its inputs and, like them, can
 * never become a Rule or Constraint.
 *
 * The result's id, style name, training-corpus label and version are the
 * caller's: nothing here guesses how two models' provenance combine.
 */

typedef enum {
    STYLE_BLEND_OK = 0,
    STYLE_BLEND_ERR_NULL_ARGUMENT,
    STYLE_BLEND_ERR_WEIGHT_ZERO_DENOMINATOR,
    STYLE_BLEND_ERR_WEIGHT_OUT_OF_RANGE, /* weight above 1 */
    STYLE_BLEND_ERR_MALFORMED,           /* a model's style_features are not a style-tendencies-v1 Value */
    STYLE_BLEND_ERR_OVERFLOW,
    STYLE_BLEND_ERR_MODEL, /* style_model_create rejected id, name, corpus label or version */
    STYLE_BLEND_ERR_ALLOC
} StyleBlendResult;

/*
 * Takes ownership of id on success only (as style_model_create does). Neither
 * input model is modified.
 */
StyleBlendResult style_model_blend(EntityID *id, const char *style_name, const char *training_corpus,
                                   const char *model_version, const StyleModel *a, const StyleModel *b,
                                   unsigned long weight_numerator, unsigned long weight_denominator,
                                   StyleModel **out);

#endif
