#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "rule_check.h"
#include "rule_table.h"

/* A table of three rules: "alpha" and "gamma" are hard, "beta" is soft. */
enum { ALPHA = 0, BETA, GAMMA, COUNT };

static const char *const NAMES[COUNT] = {"alpha", "beta", "gamma"};

static bool is_hard(int which) {
    return which != BETA;
}

static StandardCounterpointRulePair g_pairs[COUNT];
static bool g_built[COUNT];
static const StandardCounterpointRuleTable TABLE = {"test-group", NAMES, COUNT, is_hard, g_pairs, g_built};

static StandardCounterpointRulePair g_bare_pairs[COUNT];
static bool g_bare_built[COUNT];
static const StandardCounterpointRuleTable BARE = {NULL, NAMES, COUNT, NULL, g_bare_pairs, g_bare_built};

static StandardCounterpointRulePair g_soft_pairs[COUNT];
static bool g_soft_built[COUNT];
static const StandardCounterpointRuleTable SOFT = {
    "test-soft", NAMES, COUNT, standard_counterpoint_rule_table_never_hard, g_soft_pairs, g_soft_built};

/* The subject is a bit mask of the rules that hold. */
static bool holds(int which, const void *subject) {
    return (*(const unsigned *)subject & (1u << which)) != 0;
}

/* A table that registers an evaluation callback; the subject is the same bit mask of the rules that hold. */
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static StandardCounterpointRulePair g_eval_pairs[COUNT];
static bool g_eval_built[COUNT];
static const StandardCounterpointRuleTable EVAL = {"test-eval", NAMES, COUNT,         is_hard,
                                                   g_eval_pairs,  g_eval_built, evaluate_candidate, score_candidate};
STANDARD_COUNTERPOINT_RULE_TABLE_EVALUATOR(evaluate_candidate, EVAL, holds, unsigned)
STANDARD_COUNTERPOINT_RULE_TABLE_SCORER(score_candidate, EVAL, holds, unsigned)

static void test_valid_and_names(void) {
    for (int i = 0; i < COUNT; i++) {
        assert(standard_counterpoint_rule_table_valid(&TABLE, i));
        assert(strcmp(standard_counterpoint_rule_table_name(&TABLE, i), NAMES[i]) == 0);
    }
    assert(!standard_counterpoint_rule_table_valid(&TABLE, -1));
    assert(!standard_counterpoint_rule_table_valid(&TABLE, COUNT));
    assert(!standard_counterpoint_rule_table_valid(NULL, 0));
    assert(standard_counterpoint_rule_table_name(&TABLE, COUNT) == NULL);
    assert(standard_counterpoint_rule_table_name(&TABLE, -1) == NULL);
}

static void test_hardness(void) {
    assert(standard_counterpoint_rule_table_is_hard(&TABLE, ALPHA));
    assert(!standard_counterpoint_rule_table_is_hard(&TABLE, BETA));
    assert(standard_counterpoint_rule_table_is_hard(&TABLE, GAMMA));
    assert(!standard_counterpoint_rule_table_is_hard(&TABLE, COUNT));

    /* No predicate: all hard. Never hard: all soft. */
    for (int i = 0; i < COUNT; i++) {
        assert(standard_counterpoint_rule_table_is_hard(&BARE, i));
        assert(!standard_counterpoint_rule_table_is_hard(&SOFT, i));
    }
}

static const Rule *table_rule(int which) {
    return standard_counterpoint_rule_table_rule(&TABLE, which);
}

static const Constraint *table_constraint(int which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, which);
}

static void test_pairs(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(table_rule, table_constraint, COUNT);
    assert(constraint_type(table_constraint(ALPHA)) == CONSTRAINT_TYPE_HARD);
    assert(constraint_type(table_constraint(BETA)) == CONSTRAINT_TYPE_SOFT);
    assert(constraint_type(table_constraint(GAMMA)) == CONSTRAINT_TYPE_HARD);

    /* A pair is built once and cached. */
    assert(table_rule(ALPHA) == table_rule(ALPHA));
    assert(table_constraint(BETA) == table_constraint(BETA));
    assert(table_rule(ALPHA) != table_rule(GAMMA));
    assert(table_rule(-1) == NULL);

    /* Ids carry the prefix; a table without one uses the bare name. */
    assert(strcmp(entity_id_value(rule_id(table_rule(ALPHA))), "standard-counterpoint-rule-test-group-alpha") == 0);
    assert(strcmp(entity_id_value(rule_id(standard_counterpoint_rule_table_rule(&BARE, GAMMA))),
                  "standard-counterpoint-rule-gamma") == 0);

    for (int i = 0; i < COUNT; i++) {
        assert(constraint_type(standard_counterpoint_rule_table_constraint(&SOFT, i)) == CONSTRAINT_TYPE_SOFT);
        assert(constraint_type(standard_counterpoint_rule_table_constraint(&BARE, i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_check(void) {
    unsigned hard = 99, soft = 99;
    unsigned all = (1u << COUNT) - 1;

    standard_counterpoint_rule_table_check(&TABLE, holds, &all, &hard, &soft);
    assert(hard == 0 && soft == 0);

    /* Every rule fails: the hard ones in the hard mask, the soft one in the soft. */
    unsigned none = 0;
    standard_counterpoint_rule_table_check(&TABLE, holds, &none, &hard, &soft);
    assert(hard == ((1u << ALPHA) | (1u << GAMMA)));
    assert(soft == (1u << BETA));

    unsigned only_beta = 1u << BETA;
    standard_counterpoint_rule_table_check(&TABLE, holds, &only_beta, &hard, &soft);
    assert(hard == ((1u << ALPHA) | (1u << GAMMA)));
    assert(soft == 0);

    unsigned only_alpha = 1u << ALPHA;
    standard_counterpoint_rule_table_check(&TABLE, holds, &only_alpha, &hard, &soft);
    assert(hard == (1u << GAMMA));
    assert(soft == (1u << BETA));

    /* An all-soft table puts every failure in the soft mask. */
    standard_counterpoint_rule_table_check(&SOFT, holds, &none, &hard, &soft);
    assert(hard == 0);
    assert(soft == all);
}

static ConstraintEvalResult evaluate_mask(const Constraint *constraint, const char *tag, unsigned mask) {
    Value *candidate;
    assert(value_create(tag, &mask, sizeof(mask), &candidate) == VALUE_OK);
    ConstraintEvalResult result = constraint_evaluate(constraint, candidate, NULL);
    value_destroy(candidate);
    return result;
}

static void test_evaluate(void) {
    const char *tag = "standard-counterpoint/test-eval";
    char written[64];
    assert(standard_counterpoint_rule_table_candidate_type(&EVAL, written, sizeof(written)));
    assert(strcmp(written, tag) == 0);
    assert(!standard_counterpoint_rule_table_candidate_type(&EVAL, written, 8));
    assert(!standard_counterpoint_rule_table_candidate_type(&BARE, written, sizeof(written)));
    assert(!standard_counterpoint_rule_table_candidate_type(NULL, written, sizeof(written)));

    /* Only a table with a callback registers it; the others report NOT_IMPLEMENTED. */
    for (int i = 0; i < COUNT; i++) {
        assert(constraint_evaluate_fn(standard_counterpoint_rule_table_constraint(&EVAL, i)) == evaluate_candidate);
        assert(constraint_evaluate_fn(table_constraint(i)) == NULL);
        assert(evaluate_mask(table_constraint(i), tag, 0) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);
    }

    /* PASS when the rule holds, FAIL when it does not, for every rule. */
    unsigned all = (1u << COUNT) - 1;
    for (int i = 0; i < COUNT; i++) {
        const Constraint *constraint = standard_counterpoint_rule_table_constraint(&EVAL, i);
        assert(evaluate_mask(constraint, tag, all) == CONSTRAINT_EVAL_PASS);
        assert(evaluate_mask(constraint, tag, 1u << i) == CONSTRAINT_EVAL_PASS);
        assert(evaluate_mask(constraint, tag, all & ~(1u << i)) == CONSTRAINT_EVAL_FAIL);
        assert(evaluate_mask(constraint, tag, 0) == CONSTRAINT_EVAL_FAIL);
        /* A candidate of another type is NOT_APPLICABLE, not a failure. */
        assert(evaluate_mask(constraint, "standard-counterpoint/other", all) == CONSTRAINT_EVAL_NOT_APPLICABLE);
    }

    /* The right tag with content of the wrong size is rejected. */
    const Constraint *alpha = standard_counterpoint_rule_table_constraint(&EVAL, ALPHA);
    Value *wrong_size;
    assert(value_create(tag, "ab", 2, &wrong_size) == VALUE_OK);
    assert(constraint_evaluate(alpha, wrong_size, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    value_destroy(wrong_size);

    /* The helper itself: NULL arguments, a constraint that is not the table's, a table with no prefix. */
    Value *candidate;
    unsigned mask = all;
    assert(value_create(tag, &mask, sizeof(mask), &candidate) == VALUE_OK);
    assert(standard_counterpoint_rule_table_evaluate(NULL, holds, sizeof(mask), alpha, candidate) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(standard_counterpoint_rule_table_evaluate(&EVAL, NULL, sizeof(mask), alpha, candidate) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(standard_counterpoint_rule_table_evaluate(&EVAL, holds, sizeof(mask), NULL, candidate) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(standard_counterpoint_rule_table_evaluate(&EVAL, holds, sizeof(mask), alpha, NULL) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(standard_counterpoint_rule_table_evaluate(&EVAL, holds, sizeof(mask), table_constraint(ALPHA), candidate) ==
           CONSTRAINT_EVAL_NOT_IMPLEMENTED);
    assert(standard_counterpoint_rule_table_evaluate(&EVAL, holds, sizeof(mask), alpha, candidate) ==
           CONSTRAINT_EVAL_PASS);
    assert(standard_counterpoint_rule_table_evaluate(&BARE, holds, sizeof(mask),
                                                     standard_counterpoint_rule_table_constraint(&BARE, ALPHA),
                                                     candidate) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);
    value_destroy(candidate);
}

static PreferenceRankResult rank_mask(const Preference *preference, const char *tag, unsigned mask,
                                      unsigned long *numerator, unsigned long *denominator) {
    Value *candidate;
    assert(value_create(tag, &mask, sizeof(mask), &candidate) == VALUE_OK);
    PreferenceRankResult result = preference_rank(preference, candidate, NULL, numerator, denominator);
    value_destroy(candidate);
    return result;
}

static void test_preferences(void) {
    const char *tag = "standard-counterpoint/test-eval";

    /* A soft rule gets a Preference with the table's callback; a hard rule, and a table with no callback, get none. */
    for (int i = 0; i < COUNT; i++) {
        const Preference *preference = standard_counterpoint_rule_table_preference(&EVAL, i);
        assert((preference != NULL) == !is_hard(i));
        assert(standard_counterpoint_rule_table_preference(&TABLE, i) == NULL);
        assert(standard_counterpoint_rule_table_preference(&BARE, i) == NULL);
        if (preference != NULL) {
            assert(preference_score_fn(preference) == score_candidate);
        }
    }
    assert(standard_counterpoint_rule_table_preference(&EVAL, COUNT) == NULL);
    assert(standard_counterpoint_rule_table_preference(&EVAL, -1) == NULL);
    assert(standard_counterpoint_rule_table_preference(NULL, 0) == NULL);
    const Preference *beta = standard_counterpoint_rule_table_preference(&EVAL, BETA);
    assert(beta == standard_counterpoint_rule_table_preference(&EVAL, BETA));

    /* The soft rule scores 1/1 when it holds and 0/1 when it does not, and never rejects: no other result exists. */
    unsigned long numerator = 99, denominator = 99;
    assert(rank_mask(beta, tag, 1u << BETA, &numerator, &denominator) == PREFERENCE_RANK_COMPUTED);
    assert(numerator == 1 && denominator == 1);
    assert(rank_mask(beta, tag, 0, &numerator, &denominator) == PREFERENCE_RANK_COMPUTED);
    assert(numerator == 0 && denominator == 1);

    /* A candidate it cannot score (another type, wrong size) is unscored rather than given a bad score. */
    numerator = 99;
    denominator = 99;
    assert(rank_mask(beta, "standard-counterpoint/other", 1u << BETA, &numerator, &denominator) ==
           PREFERENCE_RANK_NOT_IMPLEMENTED);
    Value *wrong_size;
    assert(value_create(tag, "ab", 2, &wrong_size) == VALUE_OK);
    assert(preference_rank(beta, wrong_size, NULL, &numerator, &denominator) == PREFERENCE_RANK_NOT_IMPLEMENTED);
    value_destroy(wrong_size);
    assert(numerator == 99 && denominator == 99);

    /* The helper itself: NULL arguments, a table with no prefix. */
    unsigned mask = 1u << BETA;
    Value *candidate;
    assert(value_create(tag, &mask, sizeof(mask), &candidate) == VALUE_OK);
    PreferenceScore score = standard_counterpoint_rule_table_score(&EVAL, holds, sizeof(mask), beta, candidate);
    assert(score.numerator == 1 && score.denominator == 1);
    assert(standard_counterpoint_rule_table_score(NULL, holds, sizeof(mask), beta, candidate).denominator == 0);
    assert(standard_counterpoint_rule_table_score(&EVAL, NULL, sizeof(mask), beta, candidate).denominator == 0);
    assert(standard_counterpoint_rule_table_score(&EVAL, holds, sizeof(mask), NULL, candidate).denominator == 0);
    assert(standard_counterpoint_rule_table_score(&EVAL, holds, sizeof(mask), beta, NULL).denominator == 0);
    assert(standard_counterpoint_rule_table_score(&BARE, holds, sizeof(mask), beta, candidate).denominator == 0);
    value_destroy(candidate);
}

int main(void) {
    test_preferences();
    test_valid_and_names();
    test_hardness();
    test_pairs();
    test_check();
    test_evaluate();
    printf("test_rule_table (standard-counterpoint): ok\n");
    return 0;
}
