#ifndef URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_RULE_TABLE_H
#define URSATZ_FRAMEWORKS_SCALE_HARMONIZATION_SCALE_RULE_TABLE_H

#include <stdbool.h>

#include "scale_rule_pair.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared table for a group of rules, the same pattern as
 * standard-counterpoint's rule_table.h: the rule names, which rules are hard
 * and which soft, one cached Rule/Constraint pair for each rule
 * (scale_rule_pair.h), and a check that runs the whole group and reports
 * the rules a subject breaks as hard and soft bit masks.
 *
 * A module defines its table at file scope, with static storage for the
 * cache:
 *
 *   static ScaleRulePair g_pairs[N];
 *   static bool g_built[N];
 *   static const ScaleRuleTable TABLE = {"prefix", NAMES, N, is_hard, g_pairs, g_built};
 *
 * A pair is built the first time it is asked for, with the id
 * "<prefix>-<name>", and cached for the life of the process. Rules are
 * numbered 0 to count - 1, the values of the module's enum; every function
 * returns false or NULL for a number out of range.
 */

/* True when rule `which` is hard. */
typedef bool (*ScaleRuleHard)(int which);

/* True when the subject satisfies rule `which`. */
typedef bool (*ScaleRuleHolds)(int which, const void *subject);

typedef struct {
    const char *prefix;
    const char *const *names;
    int count;
    /* NULL when every rule is hard. */
    ScaleRuleHard is_hard;
    ScaleRulePair *pairs;
    bool *built;
} ScaleRuleTable;

/* True for a rule number from 0 to count - 1. */
bool scale_rule_table_valid(const ScaleRuleTable *table, int which);

/* True when the rule is hard; false when it is soft or the number is invalid. */
bool scale_rule_table_is_hard(const ScaleRuleTable *table, int which);

/* The rule, its constraint, and its name. NULL for an invalid number. */
const Rule *scale_rule_table_rule(const ScaleRuleTable *table, int which);
const Constraint *scale_rule_table_constraint(const ScaleRuleTable *table, int which);
const char *scale_rule_table_name(const ScaleRuleTable *table, int which);

/*
 * Runs every rule on the subject. Sets bit (1u << rule) in *hard_violations
 * for each hard rule that fails and in *soft_violations for each soft rule
 * that fails. Both outputs are written, and a rule that holds sets no bit.
 */
void scale_rule_table_check(const ScaleRuleTable *table, ScaleRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations);

/* Hardness predicate for a table whose rules are all soft. */
bool scale_rule_table_never_hard(int which);

#endif
