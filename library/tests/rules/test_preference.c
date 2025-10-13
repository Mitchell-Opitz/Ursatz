#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "context/context.h"
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
 *
 * Dispatch test: with no callback preference_rank reports
 * PREFERENCE_RANK_NOT_IMPLEMENTED; with one it reports
 * PREFERENCE_RANK_COMPUTED and the callback's exact-rational score,
 * whatever that score is (including the worst, 0/1).
 */

static const Preference *g_seen_preference;
static const Value *g_seen_candidate;
static const Context *g_seen_context;
static PreferenceScore g_score;

static PreferenceScore recording_callback(const Preference *preference, const Value *candidate,
                                          const Context *context) {
    g_seen_preference = preference;
    g_seen_candidate = candidate;
    g_seen_context = context;
    return g_score;
}

static PreferenceScore other_callback(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)candidate;
    (void)context;
    PreferenceScore score = {1, 1};
    return score;
}

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

    assert(preference_rank(preference, NULL, NULL, NULL, NULL) == PREFERENCE_RANK_ERR_NULL_CANDIDATE);

    Value *candidate;
    assert(value_create("stub-candidate", "x", 1, &candidate) == VALUE_OK);
    /* No callback registered: NOT_IMPLEMENTED, outputs untouched, and the accessor says so. */
    unsigned long numerator = 99, denominator = 99;
    assert(preference_score_fn(preference) == NULL);
    assert(preference_score_fn(NULL) == NULL);
    preference_set_score_fn(NULL, recording_callback);
    assert(preference_rank(preference, candidate, NULL, NULL, NULL) == PREFERENCE_RANK_NOT_IMPLEMENTED);
    assert(preference_rank(preference, candidate, NULL, &numerator, &denominator) == PREFERENCE_RANK_NOT_IMPLEMENTED);
    assert(numerator == 99 && denominator == 99);

    /* With a callback: dispatched with the same arguments, the score passed through exactly. */
    preference_set_score_fn(preference, recording_callback);
    assert(preference_score_fn(preference) == recording_callback);
    PreferenceScore scores[] = {{1, 1}, {3, 4}, {0, 1}, {0, 7}};
    for (size_t i = 0; i < sizeof(scores) / sizeof(scores[0]); i++) {
        g_score = scores[i];
        g_seen_preference = NULL;
        g_seen_candidate = NULL;
        g_seen_context = (const Context *)candidate;
        assert(preference_rank(preference, candidate, NULL, &numerator, &denominator) == PREFERENCE_RANK_COMPUTED);
        assert(numerator == scores[i].numerator && denominator == scores[i].denominator);
        assert(g_seen_preference == preference && g_seen_candidate == candidate && g_seen_context == NULL);
    }
    /* Either output may be omitted. */
    g_score.numerator = 1;
    g_score.denominator = 2;
    assert(preference_rank(preference, candidate, NULL, NULL, NULL) == PREFERENCE_RANK_COMPUTED);

    /* The NULL checks come before the callback. */
    g_seen_preference = NULL;
    assert(preference_rank(preference, NULL, NULL, &numerator, &denominator) == PREFERENCE_RANK_ERR_NULL_CANDIDATE);
    assert(preference_rank(NULL, candidate, NULL, &numerator, &denominator) == PREFERENCE_RANK_ERR_NULL_CANDIDATE);
    assert(g_seen_preference == NULL);

    /* A callback that cannot score the candidate (denominator 0) is NOT_IMPLEMENTED, outputs untouched. */
    numerator = 99;
    denominator = 99;
    g_score.numerator = 5;
    g_score.denominator = 0;
    assert(preference_rank(preference, candidate, NULL, &numerator, &denominator) == PREFERENCE_RANK_NOT_IMPLEMENTED);
    assert(numerator == 99 && denominator == 99);

    /* The context is passed through. */
    EntityID *context_id;
    assert(entity_id_create("context-for-preference", &context_id) == ENTITY_ID_OK);
    Context *context;
    assert(context_create(context_id, "test", NULL, &context) == CONTEXT_OK);
    g_score.numerator = 1;
    g_score.denominator = 3;
    assert(preference_rank(preference, candidate, context, &numerator, &denominator) == PREFERENCE_RANK_COMPUTED);
    assert(g_seen_context == context);
    context_destroy(context);

    /* Replacing and clearing the callback. */
    preference_set_score_fn(preference, other_callback);
    assert(preference_rank(preference, candidate, NULL, &numerator, &denominator) == PREFERENCE_RANK_COMPUTED);
    assert(numerator == 1 && denominator == 1);
    preference_set_score_fn(preference, NULL);
    assert(preference_rank(preference, candidate, NULL, &numerator, &denominator) == PREFERENCE_RANK_NOT_IMPLEMENTED);

    value_destroy(candidate);
    preference_destroy(preference);
    preference_destroy(other);
    framework_destroy(framework);

    printf("test_preference: ok\n");
    return 0;
}