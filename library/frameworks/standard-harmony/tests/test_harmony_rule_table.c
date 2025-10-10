#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/value.h"
#include "harmony_rule_table.h"

/* A table of three rules: "alpha" and "gamma" are hard, "beta" is soft. */
enum { ALPHA = 0, BETA, GAMMA, COUNT };

static const char *const NAMES[COUNT] = {"alpha", "beta", "gamma"};

static bool is_hard(int which) {
    return which != BETA;
}

/* The subject is a bit mask of the rules that hold. */
static bool holds(int which, const void *subject) {
    return (*(const unsigned *)subject & (1u << which)) != 0;
}

/* Only the alpha rule has anything to say about a subject whose top bit is set. */
static bool applies(int which, const void *subject) {
    return which != ALPHA || (*(const unsigned *)subject & 0x80u) != 0;
}

static HarmonyRulePair g_pairs[COUNT];
static bool g_built[COUNT];
static const HarmonyRuleTable PLAIN = {"test-plain", NAMES, COUNT, is_hard, g_pairs, g_built, NULL};

static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context);
static HarmonyRulePair g_eval_pairs[COUNT];
static bool g_eval_built[COUNT];
static const HarmonyRuleTable EVAL = {"test-eval", NAMES, COUNT, is_hard, g_eval_pairs, g_eval_built,
                                      evaluate_candidate};
HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, EVAL, holds, unsigned)

static HarmonyRulePair g_bare_pairs[COUNT];
static bool g_bare_built[COUNT];
static const HarmonyRuleTable BARE = {NULL, NAMES, COUNT, NULL, g_bare_pairs, g_bare_built, NULL};

static ConstraintEvalResult evaluate_mask(const Constraint *constraint, const char *tag, unsigned mask) {
    Value *candidate;
    assert(value_create(tag, &mask, sizeof(mask), &candidate) == VALUE_OK);
    ConstraintEvalResult result = constraint_evaluate(constraint, candidate, NULL);
    value_destroy(candidate);
    return result;
}

static void test_candidate_type(void) {
    char written[64];
    assert(harmony_rule_table_candidate_type(&EVAL, written, sizeof(written)));
    assert(strcmp(written, "standard-harmony/test-eval") == 0);
    assert(!harmony_rule_table_candidate_type(&EVAL, written, 8));
    assert(!harmony_rule_table_candidate_type(&BARE, written, sizeof(written)));
    assert(!harmony_rule_table_candidate_type(NULL, written, sizeof(written)));
    assert(!harmony_rule_table_candidate_type(&EVAL, NULL, sizeof(written)));
}

static void test_registration(void) {
    /* Only a table with a callback registers it; the others report NOT_IMPLEMENTED. */
    for (int i = 0; i < COUNT; i++) {
        assert(constraint_evaluate_fn(harmony_rule_table_constraint(&EVAL, i)) == evaluate_candidate);
        assert(constraint_evaluate_fn(harmony_rule_table_constraint(&PLAIN, i)) == NULL);
        assert(evaluate_mask(harmony_rule_table_constraint(&PLAIN, i), "standard-harmony/test-plain", 0) ==
               CONSTRAINT_EVAL_NOT_IMPLEMENTED);
    }
}

static void test_evaluate(void) {
    const char *tag = "standard-harmony/test-eval";
    unsigned all = (1u << COUNT) - 1;
    for (int i = 0; i < COUNT; i++) {
        const Constraint *constraint = harmony_rule_table_constraint(&EVAL, i);
        assert(evaluate_mask(constraint, tag, all) == CONSTRAINT_EVAL_PASS);
        assert(evaluate_mask(constraint, tag, 1u << i) == CONSTRAINT_EVAL_PASS);
        assert(evaluate_mask(constraint, tag, all & ~(1u << i)) == CONSTRAINT_EVAL_FAIL);
        assert(evaluate_mask(constraint, tag, 0) == CONSTRAINT_EVAL_FAIL);
        /* A candidate of another type is NOT_APPLICABLE, not a failure. */
        assert(evaluate_mask(constraint, "standard-harmony/other", all) == CONSTRAINT_EVAL_NOT_APPLICABLE);
    }

    /* The right tag with content of the wrong size is rejected. */
    const Constraint *alpha = harmony_rule_table_constraint(&EVAL, ALPHA);
    Value *wrong_size;
    assert(value_create(tag, "ab", 2, &wrong_size) == VALUE_OK);
    assert(constraint_evaluate(alpha, wrong_size, NULL) == CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    value_destroy(wrong_size);
}

static void test_helper(void) {
    const char *tag = "standard-harmony/test-eval";
    unsigned mask = 0x7u;
    Value *candidate;
    assert(value_create(tag, &mask, sizeof(mask), &candidate) == VALUE_OK);
    const Constraint *alpha = harmony_rule_table_constraint(&EVAL, ALPHA);

    assert(harmony_rule_table_evaluate(NULL, holds, NULL, sizeof(mask), alpha, candidate) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(harmony_rule_table_evaluate(&EVAL, NULL, NULL, sizeof(mask), alpha, candidate) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(harmony_rule_table_evaluate(&EVAL, holds, NULL, sizeof(mask), NULL, candidate) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(harmony_rule_table_evaluate(&EVAL, holds, NULL, sizeof(mask), alpha, NULL) ==
           CONSTRAINT_EVAL_ERR_NULL_CANDIDATE);
    assert(harmony_rule_table_evaluate(&EVAL, holds, NULL, sizeof(mask), alpha, candidate) == CONSTRAINT_EVAL_PASS);

    /* A constraint that is not the table's, and a table with no prefix, cannot be evaluated here. */
    assert(harmony_rule_table_evaluate(&EVAL, holds, NULL, sizeof(mask), harmony_rule_table_constraint(&PLAIN, ALPHA),
                                       candidate) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);
    assert(harmony_rule_table_evaluate(&BARE, holds, NULL, sizeof(mask), harmony_rule_table_constraint(&BARE, ALPHA),
                                       candidate) == CONSTRAINT_EVAL_NOT_IMPLEMENTED);

    /* A rule that does not apply to the subject is NOT_APPLICABLE; one that does is decided by holds. The beta and
       gamma rules always apply. */
    assert(harmony_rule_table_evaluate(&EVAL, holds, applies, sizeof(mask), alpha, candidate) ==
           CONSTRAINT_EVAL_NOT_APPLICABLE);
    assert(harmony_rule_table_evaluate(&EVAL, holds, applies, sizeof(mask), harmony_rule_table_constraint(&EVAL, BETA),
                                       candidate) == CONSTRAINT_EVAL_PASS);
    value_destroy(candidate);
    unsigned applicable = 0x80u | 0x1u;
    assert(value_create(tag, &applicable, sizeof(applicable), &candidate) == VALUE_OK);
    assert(harmony_rule_table_evaluate(&EVAL, holds, applies, sizeof(applicable), alpha, candidate) ==
           CONSTRAINT_EVAL_PASS);
    value_destroy(candidate);
    unsigned applicable_failing = 0x80u;
    assert(value_create(tag, &applicable_failing, sizeof(applicable_failing), &candidate) == VALUE_OK);
    assert(harmony_rule_table_evaluate(&EVAL, holds, applies, sizeof(applicable_failing), alpha, candidate) ==
           CONSTRAINT_EVAL_FAIL);
    value_destroy(candidate);
}

int main(void) {
    test_candidate_type();
    test_registration();
    test_evaluate();
    test_helper();
    printf("test_harmony_rule_table: ok\n");
    return 0;
}
