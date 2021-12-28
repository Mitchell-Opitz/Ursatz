#include <assert.h>
#include <stdio.h>
#include <string.h>

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

int main(void) {
    test_valid_and_names();
    test_hardness();
    test_pairs();
    test_check();
    printf("test_rule_table (standard-counterpoint): ok\n");
    return 0;
}
