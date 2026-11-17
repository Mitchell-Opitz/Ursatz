#ifndef URSATZ_RULES_PREFERENCE_INTERNAL_H
#define URSATZ_RULES_PREFERENCE_INTERNAL_H

#include "model/identity/entity_id.h"
#include "rules/preference.h"
#include "theory/framework.h"

/* The fields of a Preference: preference.c writes them; tests read them through tests/support/preference_test_access.h. */
struct Preference {
    EntityID *id;
    const Framework *framework;
    PreferenceScoreFn score;
};

#endif
