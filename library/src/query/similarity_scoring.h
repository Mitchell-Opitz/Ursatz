#ifndef MUSIC_QUERY_SIMILARITY_SCORING_H
#define MUSIC_QUERY_SIMILARITY_SCORING_H

#include <stddef.h>

#include "query/similarity_model.h"
#include "semantics/contour.h"

/*
 * Similarity scoring: how alike two sequences are on each declared dimension,
 * and the weighted combination. Every score is an exact rational in [0, 1]
 * (numerator/denominator in lowest terms, never float/double); no operation
 * wraps around -- an exact result that does not fit in unsigned long is
 * SIMILARITY_SCORE_ERR_OVERFLOW.
 *
 * Both sequences are plain values (SimilarityInput), so this works on any
 * sequence a caller has, persisted or not. Nothing here is a framework claim:
 * a score says how much two sequences agree on the dimensions the caller
 * chose, not that either is correct, and nothing here is or becomes a Rule.
 *
 * THE DIMENSIONS (each defined on two sequences of the same kind):
 *
 *   interval  The sequences are directed semitone intervals (signed: upward
 *             positive, downward negative, a repeated note 0). The score is
 *             the fraction of positions whose intervals are equal. It compares
 *             interval sequences, not pitches, so it is transposition-
 *             invariant: a melody and the same melody a fifth higher have the
 *             same intervals and score 1. Sequences of different length score 0.
 *
 *   contour   The sequences are directions (up, down, same). The score is the
 *             fraction of positions whose directions are equal. An input's
 *             directions are its `directions` array if it has one, otherwise
 *             derived from its intervals (positive up, negative down, zero
 *             same). Different lengths score 0.
 *
 *   rhythm    The sequences are durations, each a positive ratio of whole
 *             notes (numerator/denominator). The score compares durations as
 *             ratios: the fraction of the n - 1 consecutive duration ratios
 *             d[i+1] / d[i] that are equal. Because a ratio does not change
 *             when every duration is scaled, a uniformly augmented or
 *             diminished pattern still scores 1 on rhythm. Different counts
 *             score 0; a single duration (no ratios) scores 1 against another
 *             single duration.
 *
 * In every dimension identical sequences score 1 and sequences that disagree at
 * every position score 0.
 *
 * THE COMBINED SCORE over a SimilarityModel is
 *     sum(weight[d] * score[d]) / sum(weight[d])
 * over the declared dimensions with a non-zero weight, so it stays in [0, 1],
 * is 1 when every counted dimension is 1, and is 0 when every counted dimension
 * is 0. A dimension that is not declared, or has weight 0, contributes nothing
 * -- its sequence is not even read. A counted dimension needs a non-empty
 * sequence in both inputs (SIMILARITY_SCORE_ERR_INVALID_INPUT otherwise).
 */

typedef struct {
    unsigned long numerator;
    unsigned long denominator; /* always positive */
} SimilarityScore;

/* One duration, as a positive ratio of whole notes. */
typedef struct {
    unsigned long numerator;
    unsigned long denominator;
} SimilarityDuration;

/* A sequence to compare. Each array is borrowed and may be NULL with a count of 0 when that dimension is not used. */
typedef struct {
    const long *intervals;
    size_t interval_count;
    const ContourDirection *directions;
    size_t direction_count;
    const SimilarityDuration *durations;
    size_t duration_count;
} SimilarityInput;

typedef enum {
    SIMILARITY_SCORE_OK = 0,
    SIMILARITY_SCORE_ERR_NULL_ARGUMENT,
    SIMILARITY_SCORE_ERR_INVALID_INPUT, /* an empty sequence on a counted dimension, or a duration with a zero part */
    SIMILARITY_SCORE_ERR_OVERFLOW,
    SIMILARITY_SCORE_ERR_ALLOC /* no memory for the directions derived from intervals */
} SimilarityScoreResult;

typedef struct {
    SimilarityDimension dimension;
    SimilarityScore score;
} SimilarityDimensionScore;

/* The combined score and the per-dimension scores it was made from (the evidence), in the model's dimension order. */
typedef struct {
    SimilarityScore similarity;
    SimilarityDimensionScore dimension_scores[SIMILARITY_DIMENSION_COUNT];
    size_t dimension_score_count;
} SimilarityBreakdown;

/* Each dimension on its own; the outputs are untouched on any result other than OK. */
SimilarityScoreResult similarity_interval_score(const long *a, size_t a_count, const long *b, size_t b_count,
                                                SimilarityScore *out);
SimilarityScoreResult similarity_contour_score(const ContourDirection *a, size_t a_count, const ContourDirection *b,
                                               size_t b_count, SimilarityScore *out);
SimilarityScoreResult similarity_rhythm_score(const SimilarityDuration *a, size_t a_count,
                                              const SimilarityDuration *b, size_t b_count, SimilarityScore *out);

/* The direction of each interval: positive up, negative down, zero same. out has room for count entries. */
void similarity_directions_from_intervals(const long *intervals, size_t count, ContourDirection *out);

/* The combined score of two inputs under a model, with its evidence; *out is untouched on any result other than OK. */
SimilarityScoreResult similarity_score(const SimilarityModel *model, const SimilarityInput *a, const SimilarityInput *b,
                                       SimilarityBreakdown *out);

#endif
