#include "rules/rule.h"

#include <stdlib.h>

struct Rule {
    EntityID *id;
    const Framework *framework;
    Value *scope;
};

RuleResult rule_create(EntityID *id, const Framework *framework, Value *scope, Rule **out) {
    if (id == NULL) {
        return RULE_ERR_NULL_ID;
    }
    if (framework == NULL) {
        return RULE_ERR_NULL_FRAMEWORK;
    }

    Rule *rule = (Rule *)malloc(sizeof(Rule));
    if (rule == NULL) {
        return RULE_ERR_ALLOC;
    }

    rule->id = id;
    rule->framework = framework;
    rule->scope = scope;

    *out = rule;
    return RULE_OK;
}

void rule_destroy(Rule *rule) {
    if (rule == NULL) {
        return;
    }
    entity_id_destroy(rule->id);
    value_destroy(rule->scope);
    free(rule);
}

const EntityID *rule_id(const Rule *rule) {
    return rule->id;
}

const Framework *rule_framework(const Rule *rule) {
    return rule->framework;
}

const Value *rule_scope(const Rule *rule) {
    return rule->scope;
}

bool rule_identity_equals(const Rule *a, const Rule *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}