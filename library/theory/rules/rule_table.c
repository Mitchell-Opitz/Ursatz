#include "theory/rules/rule_table.h"

#include <stdio.h>

bool rule_table_valid(const RuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool rule_table_is_hard(const RuleTable *table, int which) {
    if (!rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool rule_table_never_hard(int which) {
    (void)which;
    return false;
}

typedef struct {
    const RuleTable *table;
    int which;
} BuildPairArg;

static void build_pair(void *arg) {
    const RuleTable *table = ((const BuildPairArg *)arg)->table;
    int which = ((const BuildPairArg *)arg)->which;
    char name[96];
    if (table->prefix != NULL) {
        snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    } else {
        snprintf(name, sizeof(name), "%s", table->names[which]);
    }
    rule_pair_build(
        table->framework != NULL ? table->framework() : NULL, table->id_prefix, name,
        rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT, &table->pairs[which]);
}

static void ensure_built(const RuleTable *table, int which) {
    if (!rule_table_valid(table, which)) {
        return;
    }
    BuildPairArg arg = {table, which};
    ursatz_once(&table->built[which], build_pair, &arg);
}

const char *rule_table_name(const RuleTable *table, int which) {
    return rule_table_valid(table, which) ? table->names[which] : NULL;
}

void rule_table_check(const RuleTable *table, RuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
