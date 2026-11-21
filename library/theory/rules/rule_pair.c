#include "theory/rules/rule_pair.h"

#include <stdio.h>

#include "model/identity/entity_id.h"

bool rule_pair_build(const Framework *framework, const char *id_prefix, const char *name, ConstraintType type,
                     RulePair *out) {
    if (type == CONSTRAINT_TYPE_WEIGHTED) {
        return false;
    }
    if (framework == NULL) {
        return false;
    }

    char id_value[256];

    snprintf(id_value, sizeof(id_value), "%s-rule-%s", id_prefix, name);
    EntityID *rule_id;
    if (entity_id_create(id_value, &rule_id) != ENTITY_ID_OK) {
        return false;
    }
    Rule *rule;
    if (rule_create(rule_id, framework, NULL, &rule) != RULE_OK) {
        entity_id_destroy(rule_id);
        return false;
    }

    snprintf(id_value, sizeof(id_value), "%s-constraint-%s", id_prefix, name);
    EntityID *constraint_id;
    if (entity_id_create(id_value, &constraint_id) != ENTITY_ID_OK) {
        rule_destroy(rule);
        return false;
    }
    Constraint *constraint;
    if (constraint_create(constraint_id, framework, NULL, type, 0, 0, NULL, &constraint) != CONSTRAINT_OK) {
        entity_id_destroy(constraint_id);
        rule_destroy(rule);
        return false;
    }

    out->rule = rule;
    out->constraint = constraint;
    out->preference = NULL;
    return true;
}
