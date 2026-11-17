#ifndef URSATZ_TESTS_SUPPORT_PREFERENCE_TEST_ACCESS_H
#define URSATZ_TESTS_SUPPORT_PREFERENCE_TEST_ACCESS_H

#include <stdlib.h>

#include "model/identity/entity_id.h"
#include "theory/rules/preference.h"
#include "theory/rules/preference_internal.h"

/*
 * What the tests read of a Preference: its framework, its scoring callback and the rank the callback gives a candidate.
 * No application reads these, so they are not in rules/preference.h.
 */

typedef enum {
    PREFERENCE_RANK_COMPUTED = 0,
    PREFERENCE_RANK_ERR_NULL_CANDIDATE,
    PREFERENCE_RANK_NOT_IMPLEMENTED
} PreferenceRankResult;

static inline void preference_destroy(Preference *preference) {
    if (preference == NULL) {
        return;
    }
    entity_id_destroy(preference->id);
    free(preference);
}

static inline const Framework *preference_framework(const Preference *preference) {
    return preference->framework;
}

static inline PreferenceScoreFn preference_score_fn(const Preference *preference) {
    return preference == NULL ? NULL : preference->score;
}

/*
 * Ranks candidate by calling the scoring callback. On PREFERENCE_RANK_COMPUTED, out_numerator/out_denominator hold the
 * callback's score (each is written only if non-NULL). PREFERENCE_RANK_ERR_NULL_CANDIDATE for a NULL preference or
 * candidate, PREFERENCE_RANK_NOT_IMPLEMENTED when no callback is registered or it could not score the candidate (a
 * score with denominator 0); the outputs are left untouched in both cases.
 */
static inline PreferenceRankResult preference_rank(const Preference *preference, const Value *candidate,
                                                   const Context *context, unsigned long *out_numerator,
                                                   unsigned long *out_denominator) {
    if (preference == NULL || candidate == NULL) {
        return PREFERENCE_RANK_ERR_NULL_CANDIDATE;
    }
    if (preference->score == NULL) {
        return PREFERENCE_RANK_NOT_IMPLEMENTED;
    }
    PreferenceScore score = preference->score(preference, candidate, context);
    if (score.denominator == 0) {
        return PREFERENCE_RANK_NOT_IMPLEMENTED;
    }
    if (out_numerator != NULL) {
        *out_numerator = score.numerator;
    }
    if (out_denominator != NULL) {
        *out_denominator = score.denominator;
    }
    return PREFERENCE_RANK_COMPUTED;
}

#endif
