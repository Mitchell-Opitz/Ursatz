#include <assert.h>
#include <string.h>

#include "identity/entity_id.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "rules/rule_pair.h"
#include "theory/framework.h"

static Framework *make_framework(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-pairs", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "pairs", &framework) == FRAMEWORK_OK);
    return framework;
}

static void destroy_pair(RulePair *pair) {
    rule_destroy(pair->rule);
    constraint_destroy(pair->constraint);
}

static void test_ids_framework_and_hard_type(void) {
    Framework *framework = make_framework();
    RulePair pair = {NULL, NULL};
    assert(rule_pair_build(framework, "demo", "no-leaps", &pair));
    assert(pair.rule != NULL && pair.constraint != NULL);
    assert(strcmp(entity_id_value(rule_id(pair.rule)), "demo-rule-no-leaps") == 0);
    assert(strcmp(entity_id_value(constraint_id(pair.constraint)), "demo-constraint-no-leaps") == 0);
    assert(rule_framework(pair.rule) == framework);
    assert(constraint_framework(pair.constraint) == framework);
    assert(constraint_type(pair.constraint) == CONSTRAINT_TYPE_HARD);
    destroy_pair(&pair);
    framework_destroy(framework);
}

static void test_typed_soft_and_weighted(void) {
    Framework *framework = make_framework();
    RulePair pair = {NULL, NULL};
    assert(rule_pair_build_typed(framework, "demo", "limit-leaps", CONSTRAINT_TYPE_SOFT, &pair));
    assert(constraint_type(pair.constraint) == CONSTRAINT_TYPE_SOFT);
    destroy_pair(&pair);

    RulePair untouched = {NULL, NULL};
    assert(!rule_pair_build_typed(framework, "demo", "weighted", CONSTRAINT_TYPE_WEIGHTED, &untouched));
    assert(untouched.rule == NULL && untouched.constraint == NULL);
    framework_destroy(framework);
}

static void test_null_arguments(void) {
    Framework *framework = make_framework();
    RulePair pair = {NULL, NULL};
    assert(!rule_pair_build(NULL, "demo", "x", &pair));
    assert(!rule_pair_build(framework, NULL, "x", &pair));
    assert(!rule_pair_build(framework, "demo", NULL, &pair));
    assert(!rule_pair_build(framework, "demo", "x", NULL));
    assert(pair.rule == NULL && pair.constraint == NULL);
    framework_destroy(framework);
}

int main(void) {
    test_ids_framework_and_hard_type();
    test_typed_soft_and_weighted();
    test_null_arguments();
    return 0;
}
