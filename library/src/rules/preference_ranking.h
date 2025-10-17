#ifndef MUSIC_RULES_PREFERENCE_RANKING_H
#define MUSIC_RULES_PREFERENCE_RANKING_H

#include <stddef.h>

#include "context/context.h"
#include "core/value.h"
#include "rules/preference.h"
#include "rules/preference_weight.h"

/*
 * Ranks a set of candidates by weighted soft-rule satisfaction. Ranking
 * orders candidates; it never removes one. Hard rejection belongs to
 * Constraint only (rules/constraint.h) -- nothing here, and nothing a
 * Preference's callback can return, drops or invalidates a candidate.
 *
 * Each candidate is scored by every Preference in the list through
 * preference_rank (scores in [0, 1], exact-rational). A candidate a
 * Preference cannot score (a NULL candidate, a Preference with no callback,
 * a callback that returns no score) counts as the worst score, 0/1, for that
 * Preference. A candidate's total is the weighted sum of its scores over the
 * sum of the weights (preference_fraction_weighted_total), so it stays in
 * [0, 1]; if every weight is zero, every candidate ties at 0/1.
 *
 * The result is out_order, an array of candidate_count indices into
 * candidates, best (highest total) first. It is a permutation: every input
 * index appears exactly once. Ties keep input order (the sort is stable).
 * Candidates are referenced, not copied, and none is mutated.
 *
 * On any error out_order is still filled with the input order (0, 1, ...,
 * candidate_count - 1), so a caller that ignores the error still gets every
 * candidate back.
 */

typedef struct {
    const Preference *preference;
    PreferenceFraction weight;
} PreferenceWeighted;

typedef enum {
    PREFERENCE_RANKING_OK = 0,
    PREFERENCE_RANKING_ERR_NULL_ARGUMENT,
    PREFERENCE_RANKING_ERR_ZERO_DENOMINATOR,
    PREFERENCE_RANKING_ERR_SCORE_OUT_OF_RANGE,
    PREFERENCE_RANKING_ERR_OVERFLOW,
    PREFERENCE_RANKING_ERR_ALLOC
} PreferenceRankingResult;

/*
 * candidates: candidate_count Value pointers (NULL entries allowed, ranked
 * as unscorable). preferences: preference_count weighted Preferences (each
 * Preference non-NULL; each weight needs a non-zero denominator). context is
 * optional. out_order is required, with room for candidate_count indices.
 * candidates may be NULL only if candidate_count is 0, and preferences only
 * if preference_count is 0 (then every candidate ties and the input order is
 * kept).
 *
 * Errors: NULL_ARGUMENT for a missing required pointer; ZERO_DENOMINATOR for
 * a weight with denominator 0; SCORE_OUT_OF_RANGE for a callback score above
 * 1 (numerator > denominator); OVERFLOW if an exact total does not fit in
 * unsigned long; ALLOC if scratch space could not be allocated.
 */
PreferenceRankingResult preference_rank_candidates(const Value *const *candidates, size_t candidate_count,
                                                   const PreferenceWeighted *preferences, size_t preference_count,
                                                   const Context *context, size_t *out_order);

#endif
