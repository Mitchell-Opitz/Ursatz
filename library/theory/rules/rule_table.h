#ifndef URSATZ_THEORY_RULES_RULE_TABLE_H
#define URSATZ_THEORY_RULES_RULE_TABLE_H

#include <stdbool.h>

#include "model/core/once.h"
#include <stddef.h>

#include "model/core/value.h"
#include "theory/framework/framework.h"
#include "theory/rules/constraint.h"
#include "theory/rules/preference.h"
#include "theory/rules/rule.h"
#include "theory/rules/rule_pair.h"

/*
 * Shared table for a group of rules, the same for every area (counterpoint, harmony, galant, scale harmonization). Every rule
 * module has the same parts: the rule names, which rules are hard and which soft, one cached Rule/Constraint pair for each rule
 * (rule_pair.h), and a check that runs the whole group and reports the rules a subject breaks as hard and soft bit masks. This
 * table holds those parts so a module states only what is its own: its names and its hardness.
 *
 * A module defines its table at file scope, with static storage for the cache:
 *
 *   static RulePair g_pairs[N];
 *   static UrsatzOnce g_built[N];
 *   static const RuleTable TABLE = {"standard-counterpoint", standard_counterpoint_framework, "standard-counterpoint", "prefix",
 *                                   NAMES, N, is_hard, g_pairs, g_built};
 *
 * A pair is built the first time it is asked for, with the ids "<id_prefix>-rule-<prefix>-<name>" and the like (the bare name
 * when the prefix is NULL), and cached for the life of the process. Rules are numbered 0 to count - 1, the values of the
 * module's enum; every function returns false or NULL for a number out of range.
 *
 * A table that only counts and classifies its rules (galant, scale harmonization: they use valid, is_hard and check) sets only
 * `count` and `is_hard` and leaves the rest NULL.
 */

/* True when rule `which` is hard. */
typedef bool (*RuleHard)(int which);

/* True when the subject satisfies rule `which`. The subject is whatever the module's check needs to evaluate a rule: an
   exercise, or a small struct of the exercise and the tonic. */
typedef bool (*RuleHolds)(int which, const void *subject);

typedef struct {
    /* The framework's name, for the candidate type "<framework_name>/<prefix>": "standard-counterpoint". */
    const char *framework_name;
    const Framework *(*framework)(void);
    /* The id prefix of the pairs it builds: "standard-counterpoint" gives "standard-counterpoint-rule-<name>". */
    const char *id_prefix;
    /* NULL: a rule's name is its bare name. */
    const char *prefix;
    const char *const *names;
    int count;
    /* NULL when every rule is hard. */
    RuleHard is_hard;
    RulePair *pairs;
    UrsatzOnce *built;
} RuleTable;

/* True for a rule number from 0 to count - 1. */
bool rule_table_valid(const RuleTable *table, int which);

/* True when the rule is hard; false when it is soft or the number is invalid. */
bool rule_table_is_hard(const RuleTable *table, int which);

const char *rule_table_name(const RuleTable *table, int which);

/*
 * Runs every rule on the subject. Sets bit (1u << rule) in *hard_violations for each hard rule that fails and in
 * *soft_violations for each soft rule that fails. Both outputs are written, and a rule that holds sets no bit.
 */
void rule_table_check(const RuleTable *table, RuleHolds holds, const void *subject, unsigned *hard_violations,
                      unsigned *soft_violations);

/* Hardness predicate for a table whose rules are all soft. */
bool rule_table_never_hard(int which);

#endif
