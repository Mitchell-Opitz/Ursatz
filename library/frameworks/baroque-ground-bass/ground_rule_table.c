#include "ground_rule_table.h"

#include <stdio.h>

bool ground_rule_table_valid(const GroundRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool ground_rule_table_is_hard(const GroundRuleTable *table, int which) {
    if (!ground_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool ground_rule_table_never_hard(int which) {
    (void)which;
    return false;
}

typedef struct {
    const GroundRuleTable *table;
    int which;
} BuildPairArg;

static void build_pair(void *arg) {
    const GroundRuleTable *table = ((const BuildPairArg *)arg)->table;
    int which = ((const BuildPairArg *)arg)->which;
    char name[96];
    snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    ground_build_rule_pair(
        name, ground_rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
        &table->pairs[which]);
}

static void ensure_built(const GroundRuleTable *table, int which) {
    if (!ground_rule_table_valid(table, which)) {
        return;
    }
    BuildPairArg arg = {table, which};
    ursatz_once(&table->built[which], build_pair, &arg);
}

const Rule *ground_rule_table_rule(const GroundRuleTable *table, int which) {
    ensure_built(table, which);
    return ground_rule_table_valid(table, which) ? table->pairs[which].rule : NULL;
}

const Constraint *ground_rule_table_constraint(const GroundRuleTable *table, int which) {
    ensure_built(table, which);
    return ground_rule_table_valid(table, which) ? table->pairs[which].constraint : NULL;
}

const char *ground_rule_table_name(const GroundRuleTable *table, int which) {
    return ground_rule_table_valid(table, which) ? table->names[which] : NULL;
}

void ground_rule_table_check(const GroundRuleTable *table, GroundRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (ground_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
