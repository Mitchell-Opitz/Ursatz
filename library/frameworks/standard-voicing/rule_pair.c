#include "rule_pair.h"

#include <stdio.h>

#include "framework.h"
#include "identity/entity_id.h"

bool standard_voicing_build_rule_pair(const char *name, ConstraintType type, StandardVoicingRulePair *out) {
    if (type == CONSTRAINT_TYPE_WEIGHTED) {
        return false;
    }
    const Framework *framework = standard_voicing_framework();
    if (framework == NULL) {
        return false;
    }

    char id_value[256];

    snprintf(id_value, sizeof(id_value), "standard-voicing-rule-%s", name);
    EntityID *rule_id;
    if (entity_id_create(id_value, &rule_id) != ENTITY_ID_OK) {
        return false;
    }
    Rule *rule;
    if (rule_create(rule_id, framework, NULL, &rule) != RULE_OK) {
        entity_id_destroy(rule_id);
        return false;
    }

    snprintf(id_value, sizeof(id_value), "standard-voicing-constraint-%s", name);
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
    return true;
}
