#include <assert.h>
#include <stdio.h>

#include "core/value.h"
#include "identity/entity_id.h"
#include "rules/rule.h"
#include "theory/framework.h"

/*
 * Construction test: a Rule holds identity and a framework/scope
 * association -- rule_framework returns the exact Framework it was
 * created with, and scope round-trips when supplied.
 */

int main(void) {
    EntityID *framework_id;
    assert(entity_id_create("framework-species-counterpoint", &framework_id) == ENTITY_ID_OK);
    Framework *framework;
    assert(framework_create(framework_id, "species counterpoint", &framework) == FRAMEWORK_OK);

    EntityID *id;
    assert(entity_id_create("rule-no-parallel-fifths", &id) == ENTITY_ID_OK);

    Rule *rule;
    assert(rule_create(id, framework, NULL, &rule) == RULE_OK);
    assert(rule_framework(rule) == framework);
    assert(rule_scope(rule) == NULL);

    EntityID *other_id;
    assert(entity_id_create("rule-other", &other_id) == ENTITY_ID_OK);
    Rule *other;
    assert(rule_create(other_id, framework, NULL, &other) == RULE_OK);

    assert(rule_identity_equals(rule, rule));
    assert(!rule_identity_equals(rule, other));

    rule_destroy(rule);
    rule_destroy(other);
    framework_destroy(framework);

    printf("test_rule: ok\n");
    return 0;
}