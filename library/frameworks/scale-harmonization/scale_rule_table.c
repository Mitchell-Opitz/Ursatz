#include "scale_rule_table.h"

#include <stdio.h>

bool scale_rule_table_valid(const ScaleRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool scale_rule_table_is_hard(const ScaleRuleTable *table, int which) {
    if (!scale_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool scale_rule_table_never_hard(int which) {
    (void)which;
    return false;
}

typedef struct {
    const ScaleRuleTable *table;
    int which;
} BuildPairArg;

static void build_pair(void *arg) {
    const ScaleRuleTable *table = ((const BuildPairArg *)arg)->table;
    int which = ((const BuildPairArg *)arg)->which;
    char name[96];
    snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    scale_build_rule_pair(
        name, scale_rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
        &table->pairs[which]);
}

static void ensure_built(const ScaleRuleTable *table, int which) {
    if (!scale_rule_table_valid(table, which)) {
        return;
    }
    BuildPairArg arg = {table, which};
    ursatz_once(&table->built[which], build_pair, &arg);
}

const Rule *scale_rule_table_rule(const ScaleRuleTable *table, int which) {
    ensure_built(table, which);
    return scale_rule_table_valid(table, which) ? table->pairs[which].rule : NULL;
}

const Constraint *scale_rule_table_constraint(const ScaleRuleTable *table, int which) {
    ensure_built(table, which);
    return scale_rule_table_valid(table, which) ? table->pairs[which].constraint : NULL;
}

const char *scale_rule_table_name(const ScaleRuleTable *table, int which) {
    return scale_rule_table_valid(table, which) ? table->names[which] : NULL;
}

void scale_rule_table_check(const ScaleRuleTable *table, ScaleRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (scale_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
