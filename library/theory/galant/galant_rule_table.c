#include "theory/galant/galant_rule_table.h"

#include <stddef.h>

bool galant_rule_table_valid(const GalantRuleTable *table, int which) {
    return table != NULL && which >= 0 && which < table->count;
}

bool galant_rule_table_is_hard(const GalantRuleTable *table, int which) {
    if (!galant_rule_table_valid(table, which)) {
        return false;
    }
    return table->is_hard == NULL || table->is_hard(which);
}

void galant_rule_table_check(const GalantRuleTable *table, GalantRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations) {
    unsigned hard = 0;
    unsigned soft = 0;
    for (int i = 0; i < table->count; i++) {
        if (holds(i, subject)) {
            continue;
        }
        if (galant_rule_table_is_hard(table, i)) {
            hard |= 1u << i;
        } else {
            soft |= 1u << i;
        }
    }
    *hard_violations = hard;
    *soft_violations = soft;
}
