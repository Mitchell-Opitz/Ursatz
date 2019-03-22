#include <assert.h>
#include <stdio.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "rules/preference.h"
#include "theory/framework.h"

/*
 * Construction test: a Preference holds identity and a framework
 * association.
 *
 * Structural test: preference_rank has no PASS/FAIL/invalid return
 * value -- PreferenceRankResult only ever reports a computed rank or
 * that ranking is not implemented, never that a candidate is rejected
 * or invalid (that vocabulary belongs to Constraint, rules/constraint.h,
 * not Preference).
 */

int main(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-species-counterpoint", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "species counterpoint", &framework) == FRAMEWORK_OK);

    EntityID *id;
    assert(entity_id_create("preference-favor-contrary-motion", &id) == ENTITY_ID_OK);
    Preference *preference;
    assert(preference_create(id, framework, &preference) == PREFERENCE_OK);
    assert(preference_framework(preference) == framework);

    EntityID *other_id;
    assert(entity_id_create("preference-other", &other_id) == ENTITY_ID_OK);
    Preference *other;
    assert(preference_create(other_id, framework, &other) == PREFERENCE_OK);

    assert(preference_identity_equals(preference, preference));
    assert(!preference_identity_equals(preference, other));

    assert(preference_rank(preference, NULL, NULL, NULL) == PREFERENCE_RANK_ERR_NULL_CANDIDATE);

    Value *candidate;
    assert(value_create("stub-candidate", "x", 1, &candidate) == VALUE_OK);
    assert(preference_rank(preference, candidate, NULL, NULL) == PREFERENCE_RANK_NOT_IMPLEMENTED);

    value_destroy(candidate);
    preference_destroy(preference);
    preference_destroy(other);
    framework_destroy(framework);

    printf("test_preference: ok\n");
    return 0;
}