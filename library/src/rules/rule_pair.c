#include "rules/rule_pair.h"

#include <stdio.h>
#include <stdlib.h>

#include "identity/entity_id.h"

bool rule_pair_build(const Framework *framework, const char *prefix, const char *name, RulePair *out) {
    return rule_pair_build_typed(framework, prefix, name, CONSTRAINT_TYPE_HARD, out);
}

bool rule_pair_build_typed(const Framework *framework, const char *prefix, const char *name, ConstraintType type,
                           RulePair *out) {
    if (framework == NULL || prefix == NULL || name == NULL || out == NULL || type == CONSTRAINT_TYPE_WEIGHTED) {
        return false;
    }

    char id_value[256];

    snprintf(id_value, sizeof(id_value), "%s-rule-%s", prefix, name);
    EntityID *rule_id;
    if (entity_id_create(id_value, &rule_id) != ENTITY_ID_OK) {
        return false;
    }
    Rule *rule;
    if (rule_create(rule_id, framework, NULL, &rule) != RULE_OK) {
        entity_id_destroy(rule_id);
        return false;
    }

    snprintf(id_value, sizeof(id_value), "%s-constraint-%s", prefix, name);
    EntityID *constraint_id;
    if (entity_id_create(id_value, &constraint_id) != ENTITY_ID_OK) {
        rule_destroy(rule);
        return false;
    }
    Constraint *constraint;
    if (constraint_create(constraint_id, framework, NULL, type, 0, 0, NULL, &constraint) !=
        CONSTRAINT_OK) {
        entity_id_destroy(constraint_id);
        rule_destroy(rule);
        return false;
    }

    out->rule = rule;
    out->constraint = constraint;
    return true;
}

static void build_table(void *arg) {
    RulePairTable *table = arg;
    if (table->count == 0 || table->framework == NULL) {
        return;
    }
    RulePair *pairs = calloc(table->count, sizeof(RulePair));
    if (pairs == NULL) {
        return;
    }
    const Framework *framework = table->framework();
    for (size_t i = 0; i < table->count; i++) {
        rule_pair_build_typed(framework, table->prefix, table->names[i], table->type, &pairs[i]);
    }
    table->pairs = pairs;
}

const RulePair *rule_pair_table_get(RulePairTable *table, size_t which) {
    if (table == NULL || which >= table->count) {
        return NULL;
    }
    ursatz_once(&table->once, build_table, table);
    if (table->pairs == NULL || table->pairs[which].rule == NULL) {
        return NULL;
    }
    return &table->pairs[which];
}
