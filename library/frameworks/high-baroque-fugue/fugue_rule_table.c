#include "fugue_rule_table.h"

#include <stdio.h>

bool fugue_rule_table_valid(const FugueRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool fugue_rule_table_is_hard(const FugueRuleTable *table, int which) {
    if (!fugue_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool fugue_rule_table_never_hard(int which) {
    (void)which;
    return false;
}

static void ensure_built(const FugueRuleTable *table, int which) {
    if (!fugue_rule_table_valid(table, which) || table->built[which]) {
        return;
    }
    table->built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    fugue_build_rule_pair(
        name, fugue_rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
        &table->pairs[which]);
}

const Rule *fugue_rule_table_rule(const FugueRuleTable *table, int which) {
    ensure_built(table, which);
    return fugue_rule_table_valid(table, which) ? table->pairs[which].rule : NULL;
}

const Constraint *fugue_rule_table_constraint(const FugueRuleTable *table, int which) {
    ensure_built(table, which);
    return fugue_rule_table_valid(table, which) ? table->pairs[which].constraint : NULL;
}

const char *fugue_rule_table_name(const FugueRuleTable *table, int which) {
    return fugue_rule_table_valid(table, which) ? table->names[which] : NULL;
}

void fugue_rule_table_check(const FugueRuleTable *table, FugueRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (fugue_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
