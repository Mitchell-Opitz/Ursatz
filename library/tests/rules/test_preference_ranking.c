#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "rules/preference.h"
#include "rules/preference_ranking.h"
#include "theory/framework.h"

/*
 * Ranking orders candidates but never removes one: the output is always a
 * permutation of the input indices, whatever the scores -- including every
 * score being zero, a Preference whose callback gives the worst possible
 * score, a Preference that cannot score at all, and every error path. Ties
 * keep input order, and weights are exact.
 *
 * A candidate here is a Value holding one unsigned long, its "quality" from
 * 0 to 10.
 */

static unsigned long quality(const Value *candidate) {
    size_t size;
    const unsigned long *content = value_content(candidate, &size);
    assert(size == sizeof(unsigned long));
    return *content;
}

/* Score = quality / 10. */
static PreferenceScore higher_is_better(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)context;
    PreferenceScore score = {quality(candidate), 10};
    return score;
}

/* Score = (10 - quality) / 10. */
static PreferenceScore lower_is_better(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)context;
    PreferenceScore score = {10 - quality(candidate), 10};
    return score;
}

/* The worst possible score for every candidate. */
static PreferenceScore always_worst(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)candidate;
    (void)context;
    PreferenceScore score = {0, 1};
    return score;
}

/* The best possible score for every candidate. */
static PreferenceScore always_best(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)candidate;
    (void)context;
    PreferenceScore score = {1, 1};
    return score;
}

/* No score at all (denominator 0): the Preference cannot judge this candidate. */
static PreferenceScore cannot_score(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)candidate;
    (void)context;
    PreferenceScore score = {1, 0};
    return score;
}

/* A score above 1, which is not a valid score. */
static PreferenceScore above_one(const Preference *preference, const Value *candidate, const Context *context) {
    (void)preference;
    (void)candidate;
    (void)context;
    PreferenceScore score = {3, 2};
    return score;
}

static Framework *g_framework;
static unsigned g_counter;

static Preference *make_preference(PreferenceScoreFn score) {
    char id_value[64];
    snprintf(id_value, sizeof(id_value), "preference-ranking-%u", g_counter++);
    EntityID *id;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    Preference *preference;
    assert(preference_create(id, g_framework, &preference) == PREFERENCE_OK);
    preference_set_score_fn(preference, score);
    return preference;
}

static Value *make_candidate(unsigned long q) {
    Value *value;
    assert(value_create("quality", &q, sizeof(q), &value) == VALUE_OK);
    return value;
}

static PreferenceWeighted weighted(const Preference *preference, unsigned long numerator, unsigned long denominator) {
    PreferenceWeighted entry = {preference, {numerator, denominator}};
    return entry;
}

/* Every index from 0 to count - 1 appears exactly once. */
static void assert_permutation(const size_t *order, size_t count) {
    unsigned char *seen = (unsigned char *)calloc(count == 0 ? 1 : count, 1);
    assert(seen != NULL);
    for (size_t i = 0; i < count; i++) {
        assert(order[i] < count);
        assert(!seen[order[i]]);
        seen[order[i]] = 1;
    }
    free(seen);
}

static void assert_input_order(const size_t *order, size_t count) {
    for (size_t i = 0; i < count; i++) {
        assert(order[i] == i);
    }
}

#define MAX_CANDIDATES 128

static Value *g_candidates[MAX_CANDIDATES];

static void make_candidates(const unsigned long *qualities, size_t count) {
    for (size_t i = 0; i < count; i++) {
        g_candidates[i] = make_candidate(qualities[i]);
    }
}

static void destroy_candidates(size_t count) {
    for (size_t i = 0; i < count; i++) {
        value_destroy(g_candidates[i]);
        g_candidates[i] = NULL;
    }
}

static PreferenceRankingResult rank(size_t count, const PreferenceWeighted *preferences, size_t preference_count,
                                    size_t *order) {
    return preference_rank_candidates((const Value *const *)g_candidates, count, preferences, preference_count, NULL,
                                      order);
}

static void test_orders_best_first(void) {
    Preference *higher = make_preference(higher_is_better);
    const unsigned long q[] = {3, 9, 5, 0, 10};
    make_candidates(q, 5);
    PreferenceWeighted preferences[] = {weighted(higher, 1, 1)};
    size_t order[5];
    assert(rank(5, preferences, 1, order) == PREFERENCE_RANKING_OK);
    const size_t expected[] = {4, 1, 2, 0, 3};
    for (size_t i = 0; i < 5; i++) {
        assert(order[i] == expected[i]);
    }
    assert_permutation(order, 5);
    destroy_candidates(5);
    preference_destroy(higher);
}

static void test_ties_keep_input_order(void) {
    Preference *higher = make_preference(higher_is_better);
    const unsigned long q[] = {5, 5, 7, 5, 7, 2, 5};
    make_candidates(q, 7);
    PreferenceWeighted preferences[] = {weighted(higher, 1, 1)};
    size_t order[7];
    assert(rank(7, preferences, 1, order) == PREFERENCE_RANKING_OK);
    const size_t expected[] = {2, 4, 0, 1, 3, 6, 5};
    for (size_t i = 0; i < 7; i++) {
        assert(order[i] == expected[i]);
    }
    destroy_candidates(7);
    preference_destroy(higher);
}

static void test_a_large_set_is_stable_and_complete(void) {
    Preference *higher = make_preference(higher_is_better);
    unsigned long q[MAX_CANDIDATES];
    for (size_t i = 0; i < MAX_CANDIDATES; i++) {
        q[i] = (i * 7) % 11; /* plenty of duplicates */
    }
    make_candidates(q, MAX_CANDIDATES);
    PreferenceWeighted preferences[] = {weighted(higher, 2, 3)};
    size_t order[MAX_CANDIDATES];
    assert(rank(MAX_CANDIDATES, preferences, 1, order) == PREFERENCE_RANKING_OK);
    assert_permutation(order, MAX_CANDIDATES);
    for (size_t i = 1; i < MAX_CANDIDATES; i++) {
        unsigned long before = q[order[i - 1]], after = q[order[i]];
        assert(before >= after);
        if (before == after) {
            assert(order[i - 1] < order[i]);
        }
    }
    destroy_candidates(MAX_CANDIDATES);
    preference_destroy(higher);
}

static void test_weights_decide_between_preferences(void) {
    Preference *higher = make_preference(higher_is_better);
    Preference *lower = make_preference(lower_is_better);
    const unsigned long q[] = {2, 8, 5};
    make_candidates(q, 3);
    size_t order[3];

    PreferenceWeighted favor_higher[] = {weighted(higher, 3, 1), weighted(lower, 1, 1)};
    assert(rank(3, favor_higher, 2, order) == PREFERENCE_RANKING_OK);
    assert(order[0] == 1 && order[1] == 2 && order[2] == 0);

    PreferenceWeighted favor_lower[] = {weighted(higher, 1, 3), weighted(lower, 1, 1)};
    assert(rank(3, favor_lower, 2, order) == PREFERENCE_RANKING_OK);
    assert(order[0] == 0 && order[1] == 2 && order[2] == 1);

    /* Equal weights cancel out exactly: every candidate ties, so the input order stays. */
    PreferenceWeighted balanced[] = {weighted(higher, 1, 2), weighted(lower, 2, 4)};
    assert(rank(3, balanced, 2, order) == PREFERENCE_RANKING_OK);
    assert_input_order(order, 3);

    /* A weight of zero removes that preference. */
    PreferenceWeighted only_lower[] = {weighted(higher, 0, 1), weighted(lower, 1, 1)};
    assert(rank(3, only_lower, 2, order) == PREFERENCE_RANKING_OK);
    assert(order[0] == 0 && order[1] == 2 && order[2] == 1);

    destroy_candidates(3);
    preference_destroy(higher);
    preference_destroy(lower);
}

static void test_ranking_never_drops_a_candidate(void) {
    const unsigned long q[] = {4, 4, 0, 10, 7, 7};
    make_candidates(q, 6);
    size_t order[6];

    /* Every score zero: nothing is removed, and the input order is kept. */
    Preference *worst = make_preference(always_worst);
    PreferenceWeighted worst_only[] = {weighted(worst, 1, 1)};
    assert(rank(6, worst_only, 1, order) == PREFERENCE_RANKING_OK);
    assert_permutation(order, 6);
    assert_input_order(order, 6);

    /* The worst possible score from one Preference does not remove a candidate either. */
    Preference *higher = make_preference(higher_is_better);
    PreferenceWeighted with_worst[] = {weighted(higher, 1, 1), weighted(worst, 5, 1)};
    assert(rank(6, with_worst, 2, order) == PREFERENCE_RANKING_OK);
    assert_permutation(order, 6);
    assert(order[0] == 3);

    /* A Preference that cannot score, and one with no callback at all, count as the worst score. */
    Preference *cannot = make_preference(cannot_score);
    Preference *none = make_preference(NULL);
    PreferenceWeighted unscorable[] = {weighted(cannot, 1, 1), weighted(none, 1, 1)};
    assert(rank(6, unscorable, 2, order) == PREFERENCE_RANKING_OK);
    assert_permutation(order, 6);
    assert_input_order(order, 6);

    /* Every score the best possible: all tie. */
    Preference *best = make_preference(always_best);
    PreferenceWeighted best_only[] = {weighted(best, 3, 7)};
    assert(rank(6, best_only, 1, order) == PREFERENCE_RANKING_OK);
    assert_input_order(order, 6);

    /* All weights zero, or no Preferences at all: all tie at the worst score. */
    PreferenceWeighted zero_weights[] = {weighted(higher, 0, 1), weighted(best, 0, 5)};
    assert(rank(6, zero_weights, 2, order) == PREFERENCE_RANKING_OK);
    assert_input_order(order, 6);
    assert(rank(6, NULL, 0, order) == PREFERENCE_RANKING_OK);
    assert_input_order(order, 6);

    destroy_candidates(6);
    preference_destroy(worst);
    preference_destroy(higher);
    preference_destroy(cannot);
    preference_destroy(none);
    preference_destroy(best);
}

static void test_null_candidates_are_ranked_last_not_removed(void) {
    Preference *higher = make_preference(higher_is_better);
    g_candidates[0] = NULL;
    g_candidates[1] = make_candidate(1);
    g_candidates[2] = NULL;
    g_candidates[3] = make_candidate(9);
    PreferenceWeighted preferences[] = {weighted(higher, 1, 1)};
    size_t order[4];
    assert(rank(4, preferences, 1, order) == PREFERENCE_RANKING_OK);
    assert_permutation(order, 4);
    assert(order[0] == 3 && order[1] == 1 && order[2] == 0 && order[3] == 2);
    value_destroy(g_candidates[1]);
    value_destroy(g_candidates[3]);
    preference_destroy(higher);
}

static void test_empty_and_single(void) {
    Preference *higher = make_preference(higher_is_better);
    PreferenceWeighted preferences[] = {weighted(higher, 1, 1)};
    size_t order[1] = {99};
    assert(preference_rank_candidates(NULL, 0, preferences, 1, NULL, order) == PREFERENCE_RANKING_OK);
    assert(order[0] == 99);
    g_candidates[0] = make_candidate(3);
    assert(rank(1, preferences, 1, order) == PREFERENCE_RANKING_OK);
    assert(order[0] == 0);
    value_destroy(g_candidates[0]);
    preference_destroy(higher);
}

static void test_errors_still_return_every_candidate(void) {
    Preference *higher = make_preference(higher_is_better);
    Preference *over = make_preference(above_one);
    const unsigned long q[] = {1, 9, 5, 7};
    make_candidates(q, 4);
    size_t order[4];

    /* A weight with a zero denominator. */
    PreferenceWeighted zero_denominator[] = {weighted(higher, 1, 0)};
    assert(rank(4, zero_denominator, 1, order) == PREFERENCE_RANKING_ERR_ZERO_DENOMINATOR);
    assert_input_order(order, 4);

    /* A callback score above 1. */
    PreferenceWeighted too_high[] = {weighted(higher, 1, 1), weighted(over, 1, 1)};
    assert(rank(4, too_high, 2, order) == PREFERENCE_RANKING_ERR_SCORE_OUT_OF_RANGE);
    assert_permutation(order, 4);
    assert_input_order(order, 4);

    /* Weights whose exact sum does not fit. */
    PreferenceWeighted overflow[] = {weighted(higher, ULONG_MAX, 1), weighted(higher, 1, 1)};
    assert(rank(4, overflow, 2, order) == PREFERENCE_RANKING_ERR_OVERFLOW);
    assert_input_order(order, 4);

    /* Missing arguments. */
    PreferenceWeighted null_preference[] = {weighted(NULL, 1, 1)};
    assert(rank(4, null_preference, 1, order) == PREFERENCE_RANKING_ERR_NULL_ARGUMENT);
    assert_input_order(order, 4);
    PreferenceWeighted fine[] = {weighted(higher, 1, 1)};
    assert(preference_rank_candidates(NULL, 4, fine, 1, NULL, order) == PREFERENCE_RANKING_ERR_NULL_ARGUMENT);
    assert_input_order(order, 4);
    assert(preference_rank_candidates((const Value *const *)g_candidates, 4, NULL, 1, NULL, order) ==
           PREFERENCE_RANKING_ERR_NULL_ARGUMENT);
    assert_input_order(order, 4);
    assert(preference_rank_candidates((const Value *const *)g_candidates, 4, fine, 1, NULL, NULL) ==
           PREFERENCE_RANKING_ERR_NULL_ARGUMENT);

    /* Ranking after an error works as usual. */
    assert(rank(4, fine, 1, order) == PREFERENCE_RANKING_OK);
    assert(order[0] == 1 && order[1] == 3 && order[2] == 2 && order[3] == 0);

    destroy_candidates(4);
    preference_destroy(higher);
    preference_destroy(over);
}

static void test_candidates_are_not_modified(void) {
    Preference *higher = make_preference(higher_is_better);
    const unsigned long q[] = {6, 2, 9};
    make_candidates(q, 3);
    PreferenceWeighted preferences[] = {weighted(higher, 1, 1)};
    size_t order[3];
    assert(rank(3, preferences, 1, order) == PREFERENCE_RANKING_OK);
    for (size_t i = 0; i < 3; i++) {
        assert(quality(g_candidates[i]) == q[i]);
    }
    destroy_candidates(3);
    preference_destroy(higher);
}

int main(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-ranking", &framework_id) == ENTITY_ID_OK);
    assert(framework_create(framework_id, "ranking", &g_framework) == FRAMEWORK_OK);

    test_orders_best_first();
    test_ties_keep_input_order();
    test_a_large_set_is_stable_and_complete();
    test_weights_decide_between_preferences();
    test_ranking_never_drops_a_candidate();
    test_null_candidates_are_ranked_last_not_removed();
    test_empty_and_single();
    test_errors_still_return_every_candidate();
    test_candidates_are_not_modified();

    framework_destroy(g_framework);
    printf("test_preference_ranking: ok\n");
    return 0;
}
