#include "rules/preference_ranking.h"

#include <stdlib.h>

static void fill_input_order(size_t *order, size_t count) {
    for (size_t i = 0; i < count; i++) {
        order[i] = i;
    }
}

static PreferenceRankingResult from_weight_result(PreferenceWeightResult status) {
    switch (status) {
        case PREFERENCE_WEIGHT_OK:
            return PREFERENCE_RANKING_OK;
        case PREFERENCE_WEIGHT_ERR_ZERO_DENOMINATOR:
            return PREFERENCE_RANKING_ERR_ZERO_DENOMINATOR;
        case PREFERENCE_WEIGHT_ERR_OVERFLOW:
            return PREFERENCE_RANKING_ERR_OVERFLOW;
        default:
            return PREFERENCE_RANKING_ERR_NULL_ARGUMENT;
    }
}

/* One preference's score for one candidate: its rank if it computes one, else the worst score. */
static PreferenceRankingResult score_candidate(const Preference *preference, const Value *candidate,
                                               const Context *context, PreferenceFraction *out) {
    unsigned long numerator = 0, denominator = 1;
    if (candidate != NULL && preference_rank(preference, candidate, context, &numerator, &denominator) ==
                                 PREFERENCE_RANK_COMPUTED) {
        if (numerator > denominator) {
            return PREFERENCE_RANKING_ERR_SCORE_OUT_OF_RANGE;
        }
    } else {
        numerator = 0;
        denominator = 1;
    }
    out->numerator = numerator;
    out->denominator = denominator;
    return PREFERENCE_RANKING_OK;
}

/* Higher totals first; equal totals keep the lower index first. */
static int comes_first(const PreferenceFraction *totals, size_t left, size_t right) {
    int comparison = 0;
    if (preference_fraction_compare(&totals[left], &totals[right], &comparison) != PREFERENCE_WEIGHT_OK) {
        return 1;
    }
    return comparison >= 0;
}

/* Stable bottom-up merge sort of order[] by totals, using scratch[] as the merge buffer. */
static void sort_order(size_t *order, size_t *scratch, size_t count, const PreferenceFraction *totals) {
    for (size_t width = 1; width < count; width *= 2) {
        for (size_t start = 0; start < count; start += 2 * width) {
            size_t middle = start + width < count ? start + width : count;
            size_t end = start + 2 * width < count ? start + 2 * width : count;
            size_t left = start, right = middle, out = start;
            while (left < middle && right < end) {
                scratch[out++] = comes_first(totals, order[left], order[right]) ? order[left++] : order[right++];
            }
            while (left < middle) {
                scratch[out++] = order[left++];
            }
            while (right < end) {
                scratch[out++] = order[right++];
            }
        }
        for (size_t i = 0; i < count; i++) {
            order[i] = scratch[i];
        }
    }
}

PreferenceRankingResult preference_rank_candidates(const Value *const *candidates, size_t candidate_count,
                                                   const PreferenceWeighted *preferences, size_t preference_count,
                                                   const Context *context, size_t *out_order) {
    if (out_order == NULL) {
        return PREFERENCE_RANKING_ERR_NULL_ARGUMENT;
    }
    fill_input_order(out_order, candidate_count);
    if ((candidate_count > 0 && candidates == NULL) || (preference_count > 0 && preferences == NULL)) {
        return PREFERENCE_RANKING_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < preference_count; i++) {
        if (preferences[i].preference == NULL) {
            return PREFERENCE_RANKING_ERR_NULL_ARGUMENT;
        }
        if (preferences[i].weight.denominator == 0) {
            return PREFERENCE_RANKING_ERR_ZERO_DENOMINATOR;
        }
    }
    if (candidate_count == 0) {
        return PREFERENCE_RANKING_OK;
    }

    PreferenceFraction *totals = (PreferenceFraction *)malloc(candidate_count * sizeof(PreferenceFraction));
    size_t *scratch = (size_t *)malloc(candidate_count * sizeof(size_t));
    PreferenceFraction *scores = NULL;
    PreferenceFraction *weights = NULL;
    if (preference_count > 0) {
        scores = (PreferenceFraction *)malloc(preference_count * sizeof(PreferenceFraction));
        weights = (PreferenceFraction *)malloc(preference_count * sizeof(PreferenceFraction));
    }
    PreferenceRankingResult result = PREFERENCE_RANKING_OK;
    if (totals == NULL || scratch == NULL || (preference_count > 0 && (scores == NULL || weights == NULL))) {
        result = PREFERENCE_RANKING_ERR_ALLOC;
    }

    if (result == PREFERENCE_RANKING_OK) {
        for (size_t p = 0; p < preference_count; p++) {
            weights[p] = preferences[p].weight;
        }
        for (size_t c = 0; c < candidate_count && result == PREFERENCE_RANKING_OK; c++) {
            for (size_t p = 0; p < preference_count && result == PREFERENCE_RANKING_OK; p++) {
                result = score_candidate(preferences[p].preference, candidates[c], context, &scores[p]);
            }
            if (result == PREFERENCE_RANKING_OK) {
                result = from_weight_result(
                    preference_fraction_weighted_total(scores, weights, preference_count, &totals[c]));
            }
        }
    }
    if (result == PREFERENCE_RANKING_OK) {
        sort_order(out_order, scratch, candidate_count, totals);
    }

    free(totals);
    free(scratch);
    free(scores);
    free(weights);
    return result;
}
