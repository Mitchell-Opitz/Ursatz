#include <assert.h>
#include <stdio.h>

#include "model/core/value.h"
#include "model/identity/entity_id.h"
#include "rules/rule.h"
#include "theory/framework.h"
#include "framework_test_access.h"

/*
 * Construction test: a Rule is created from an id, a framework and an
 * optional scope, refuses a missing id or framework, and is destroyed
 * without destroying its framework.
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

    EntityID *scoped_id;
    assert(entity_id_create("rule-scoped", &scoped_id) == ENTITY_ID_OK);
    Value *scope;
    assert(value_create("test-scope", "x", 1, &scope) == VALUE_OK);
    Rule *scoped;
    assert(rule_create(scoped_id, framework, scope, &scoped) == RULE_OK);

    Rule *none = NULL;
    EntityID *no_framework_id;
    assert(entity_id_create("rule-no-framework", &no_framework_id) == ENTITY_ID_OK);
    assert(rule_create(NULL, framework, NULL, &none) == RULE_ERR_NULL_ID);
    assert(rule_create(no_framework_id, NULL, NULL, &none) == RULE_ERR_NULL_FRAMEWORK);
    assert(none == NULL);
    entity_id_destroy(no_framework_id);

    rule_destroy(NULL);
    rule_destroy(rule);
    rule_destroy(scoped);
    framework_destroy(framework);

    printf("test_rule: ok\n");
    return 0;
}
