#include "rules/preference.h"

#include <stdlib.h>

struct Preference {
    EntityID *id;
    const Framework *framework;
    PreferenceScoreFn score;
};

PreferenceResult preference_create(EntityID *id, const Framework *framework, Preference **out) {
    if (id == NULL) {
        return PREFERENCE_ERR_NULL_ID;
    }
    if (framework == NULL) {
        return PREFERENCE_ERR_NULL_FRAMEWORK;
    }

    Preference *preference = (Preference *)malloc(sizeof(Preference));
    if (preference == NULL) {
        return PREFERENCE_ERR_ALLOC;
    }

    preference->id = id;
    preference->framework = framework;
    preference->score = NULL;

    *out = preference;
    return PREFERENCE_OK;
}

void preference_destroy(Preference *preference) {
    if (preference == NULL) {
        return;
    }
    entity_id_destroy(preference->id);
    free(preference);
}

const EntityID *preference_id(const Preference *preference) {
    return preference->id;
}

const Framework *preference_framework(const Preference *preference) {
    return preference->framework;
}

bool preference_identity_equals(const Preference *a, const Preference *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}

void preference_set_score_fn(Preference *preference, PreferenceScoreFn score) {
    if (preference == NULL) {
        return;
    }
    preference->score = score;
}

PreferenceScoreFn preference_score_fn(const Preference *preference) {
    return preference == NULL ? NULL : preference->score;
}

PreferenceRankResult preference_rank(const Preference *preference, const Value *candidate, const Context *context,
                                     unsigned long *out_numerator, unsigned long *out_denominator) {
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
