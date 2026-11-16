#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_RULE_TABLE_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_RULE_TABLE_H

#include <stdbool.h>

/*
 * Shared table for a group of rules: how many rules there are, which are
 * hard and which soft, and a check that runs the whole group and reports
 * the rules a subject breaks as hard and soft bit masks.
 *
 * A module defines its table at file scope:
 *
 *   static const GalantRuleTable TABLE = {N, is_hard};
 *
 * Rules are numbered 0 to count - 1, the values of the module's enum; every
 * function returns false for a number out of range.
 */

/* True when rule `which` is hard. */
typedef bool (*GalantRuleHard)(int which);

/* True when the subject satisfies rule `which`. */
typedef bool (*GalantRuleHolds)(int which, const void *subject);

typedef struct {
    int count;
    /* NULL when every rule is hard. */
    GalantRuleHard is_hard;
} GalantRuleTable;

/* True for a rule number from 0 to count - 1. */
bool galant_rule_table_valid(const GalantRuleTable *table, int which);

/* True when the rule is hard; false when it is soft or the number is invalid. */
bool galant_rule_table_is_hard(const GalantRuleTable *table, int which);

/*
 * Runs every rule on the subject. Sets bit (1u << rule) in *hard_violations
 * for each hard rule that fails and in *soft_violations for each soft rule
 * that fails. Both outputs are written, and a rule that holds sets no bit.
 */
void galant_rule_table_check(const GalantRuleTable *table, GalantRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations);

#endif
