#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/rules/rule_table.h"

/* A table of three rules: "alpha" and "gamma" are hard, "beta" is soft. */
enum { ALPHA = 0, BETA, GAMMA, COUNT };

static const char *const NAMES[COUNT] = {"alpha", "beta", "gamma"};

static bool is_hard(int which) {
    return which != BETA;
}

static const RuleTable TABLE = {.name = "test-group", .names = NAMES, .count = COUNT, .is_hard = is_hard};
static const RuleTable BARE = {.names = NAMES, .count = COUNT};
static const RuleTable SOFT = {.name = "test-soft", .names = NAMES, .count = COUNT, .is_hard = rule_table_never_hard};

/* The subject is a bit mask of the rules that hold. */
static bool holds(int which, const void *subject) {
    return (*(const unsigned *)subject & (1u << which)) != 0;
}

static void test_valid_and_names(void) {
    for (int i = 0; i < COUNT; i++) {
        assert(rule_table_valid(&TABLE, i));
        assert(strcmp(rule_table_name(&TABLE, i), NAMES[i]) == 0);
    }
    assert(!rule_table_valid(&TABLE, -1));
    assert(!rule_table_valid(&TABLE, COUNT));
    assert(!rule_table_valid(NULL, 0));
    assert(rule_table_name(&TABLE, COUNT) == NULL);
    assert(rule_table_name(&TABLE, -1) == NULL);
}

static void test_hardness(void) {
    assert(rule_table_is_hard(&TABLE, ALPHA));
    assert(!rule_table_is_hard(&TABLE, BETA));
    assert(rule_table_is_hard(&TABLE, GAMMA));
    assert(!rule_table_is_hard(&TABLE, COUNT));

    /* No predicate: all hard. Never hard: all soft. */
    for (int i = 0; i < COUNT; i++) {
        assert(rule_table_is_hard(&BARE, i));
        assert(!rule_table_is_hard(&SOFT, i));
    }
}

static void test_check(void) {
    unsigned hard = 99, soft = 99;
    unsigned all = (1u << COUNT) - 1;

    rule_table_check(&TABLE, holds, &all, &hard, &soft);
    assert(hard == 0 && soft == 0);

    /* Every rule fails: the hard ones in the hard mask, the soft one in the soft. */
    unsigned none = 0;
    rule_table_check(&TABLE, holds, &none, &hard, &soft);
    assert(hard == ((1u << ALPHA) | (1u << GAMMA)));
    assert(soft == (1u << BETA));

    unsigned only_beta = 1u << BETA;
    rule_table_check(&TABLE, holds, &only_beta, &hard, &soft);
    assert(hard == ((1u << ALPHA) | (1u << GAMMA)));
    assert(soft == 0);

    unsigned only_alpha = 1u << ALPHA;
    rule_table_check(&TABLE, holds, &only_alpha, &hard, &soft);
    assert(hard == (1u << GAMMA));
    assert(soft == (1u << BETA));

    /* An all-soft table puts every failure in the soft mask. */
    rule_table_check(&SOFT, holds, &none, &hard, &soft);
    assert(hard == 0);
    assert(soft == all);
}

int main(void) {
    test_valid_and_names();
    test_hardness();
    test_check();
    printf("test_rule_table (standard-counterpoint): ok\n");
    return 0;
}
