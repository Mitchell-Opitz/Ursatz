#include "rules/preference.h"

#include <stdlib.h>

struct Preference {
    EntityID *id;
    const Framework *framework;
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

PreferenceRankResult preference_rank(const Preference *preference, const Value *candidate,
                                      unsigned long *out_numerator, unsigned long *out_denominator) {
    (void)preference;
    (void)out_numerator;
    (void)out_denominator;
    if (candidate == NULL) {
        return PREFERENCE_RANK_ERR_NULL_CANDIDATE;
    }
    return PREFERENCE_RANK_NOT_IMPLEMENTED;
}