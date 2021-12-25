#include "rule_table.h"

#include <stdio.h>

bool standard_counterpoint_rule_table_valid(const StandardCounterpointRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool standard_counterpoint_rule_table_is_hard(const StandardCounterpointRuleTable *table, int which) {
    if (!standard_counterpoint_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool standard_counterpoint_rule_table_never_hard(int which) {
    (void)which;
    return false;
}

static void ensure_built(const StandardCounterpointRuleTable *table, int which) {
    if (!standard_counterpoint_rule_table_valid(table, which) || table->built[which]) {
        return;
    }
    table->built[which] = true;
    char name[96];
    if (table->prefix != NULL) {
        snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    } else {
        snprintf(name, sizeof(name), "%s", table->names[which]);
    }
    standard_counterpoint_build_rule_pair(
        name, standard_counterpoint_rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
        &table->pairs[which]);
}

const Rule *standard_counterpoint_rule_table_rule(const StandardCounterpointRuleTable *table, int which) {
    ensure_built(table, which);
    return standard_counterpoint_rule_table_valid(table, which) ? table->pairs[which].rule : NULL;
}

const Constraint *standard_counterpoint_rule_table_constraint(const StandardCounterpointRuleTable *table, int which) {
    ensure_built(table, which);
    return standard_counterpoint_rule_table_valid(table, which) ? table->pairs[which].constraint : NULL;
}

const char *standard_counterpoint_rule_table_name(const StandardCounterpointRuleTable *table, int which) {
    return standard_counterpoint_rule_table_valid(table, which) ? table->names[which] : NULL;
}

void standard_counterpoint_rule_table_check(const StandardCounterpointRuleTable *table,
                                            StandardCounterpointRuleHolds holds, const void *subject,
                                            unsigned *hard_violations, unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (standard_counterpoint_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
