#include "rules/preference.h"

#include <stdlib.h>

#include "rules/preference_internal.h"

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

void preference_set_score_fn(Preference *preference, PreferenceScoreFn score) {
    if (preference == NULL) {
        return;
    }
    preference->score = score;
}
