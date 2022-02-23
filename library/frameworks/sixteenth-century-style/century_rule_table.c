#include "century_rule_table.h"

#include <stdio.h>

bool sixteenth_century_rule_table_valid(const SixteenthCenturyRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool sixteenth_century_rule_table_is_hard(const SixteenthCenturyRuleTable *table, int which) {
    if (!sixteenth_century_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

bool sixteenth_century_rule_table_never_hard(int which) {
    (void)which;
    return false;
}

static void ensure_built(const SixteenthCenturyRuleTable *table, int which) {
    if (!sixteenth_century_rule_table_valid(table, which) || table->built[which]) {
        return;
    }
    table->built[which] = true;
    char name[96];
    snprintf(name, sizeof(name), "%s-%s", table->prefix, table->names[which]);
    sixteenth_century_build_rule_pair(
        name, sixteenth_century_rule_table_is_hard(table, which) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT,
        &table->pairs[which]);
}

const Rule *sixteenth_century_rule_table_rule(const SixteenthCenturyRuleTable *table, int which) {
    ensure_built(table, which);
    return sixteenth_century_rule_table_valid(table, which) ? table->pairs[which].rule : NULL;
}

const Constraint *sixteenth_century_rule_table_constraint(const SixteenthCenturyRuleTable *table, int which) {
    ensure_built(table, which);
    return sixteenth_century_rule_table_valid(table, which) ? table->pairs[which].constraint : NULL;
}

const char *sixteenth_century_rule_table_name(const SixteenthCenturyRuleTable *table, int which) {
    return sixteenth_century_rule_table_valid(table, which) ? table->names[which] : NULL;
}

void sixteenth_century_rule_table_check(const SixteenthCenturyRuleTable *table, SixteenthCenturyRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (sixteenth_century_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
