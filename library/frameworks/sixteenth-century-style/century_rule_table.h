#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RULE_TABLE_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RULE_TABLE_H

#include <stdbool.h>

#include "core/once.h"

#include "century_rule_pair.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared table for a group of rules, the same pattern as
 * standard-counterpoint's rule_table.h: the rule names, which rules are hard
 * and which soft, one cached Rule/Constraint pair for each rule
 * (century_rule_pair.h), and a check that runs the whole group and reports
 * the rules a subject breaks as hard and soft bit masks.
 *
 * A module defines its table at file scope, with static storage for the
 * cache:
 *
 *   static SixteenthCenturyRulePair g_pairs[N];
 *   static UrsatzOnce g_built[N];
 *   static const SixteenthCenturyRuleTable TABLE = {"prefix", NAMES, N, is_hard, g_pairs, g_built};
 *
 * A pair is built the first time it is asked for, with the id
 * "<prefix>-<name>", and cached for the life of the process. Rules are
 * numbered 0 to count - 1, the values of the module's enum; every function
 * returns false or NULL for a number out of range.
 */

/* True when rule `which` is hard. */
typedef bool (*SixteenthCenturyRuleHard)(int which);

/* True when the subject satisfies rule `which`. */
typedef bool (*SixteenthCenturyRuleHolds)(int which, const void *subject);

typedef struct {
    const char *prefix;
    const char *const *names;
    int count;
    /* NULL when every rule is hard. */
    SixteenthCenturyRuleHard is_hard;
    SixteenthCenturyRulePair *pairs;
    UrsatzOnce *built;
} SixteenthCenturyRuleTable;

/* True for a rule number from 0 to count - 1. */
bool sixteenth_century_rule_table_valid(const SixteenthCenturyRuleTable *table, int which);

/* True when the rule is hard; false when it is soft or the number is invalid. */
bool sixteenth_century_rule_table_is_hard(const SixteenthCenturyRuleTable *table, int which);

/* The rule, its constraint, and its name. NULL for an invalid number. */
const Rule *sixteenth_century_rule_table_rule(const SixteenthCenturyRuleTable *table, int which);
const Constraint *sixteenth_century_rule_table_constraint(const SixteenthCenturyRuleTable *table, int which);
const char *sixteenth_century_rule_table_name(const SixteenthCenturyRuleTable *table, int which);

/*
 * Runs every rule on the subject. Sets bit (1u << rule) in *hard_violations
 * for each hard rule that fails and in *soft_violations for each soft rule
 * that fails. Both outputs are written, and a rule that holds sets no bit.
 */
void sixteenth_century_rule_table_check(const SixteenthCenturyRuleTable *table, SixteenthCenturyRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations);

/* Hardness predicate for a table whose rules are all soft. */
bool sixteenth_century_rule_table_never_hard(int which);

#endif
