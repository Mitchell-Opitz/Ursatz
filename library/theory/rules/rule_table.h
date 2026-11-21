#ifndef URSATZ_THEORY_RULES_RULE_TABLE_H
#define URSATZ_THEORY_RULES_RULE_TABLE_H

#include <stdbool.h>

/*
 * Shared table for a group of rules, the same for every area (counterpoint, harmony, galant, scale harmonization). Every rule
 * module has the same parts: the rule names, which rules are soft (the rest are hard), and a check that runs the whole group and
 * reports the rules a subject breaks as hard and soft bit masks. This table holds those parts so a module states only what is its
 * own: its names and which of its rules are soft.
 *
 * A module defines its table at file scope, with the soft rules as a bool table indexed by the module's enum:
 *
 *   static const bool SOFT[N] = {[SOME_RULE] = true};
 *   static const RuleTable TABLE = {.name = "prefix", .names = NAMES, .soft = SOFT, .count = N};
 *
 * Rules are numbered 0 to count - 1, the values of the module's enum; every function returns false or NULL for a number out of
 * range. A table that only counts and classifies its rules (galant, scale harmonization: they use valid, is_hard and check) sets
 * only `soft` and `count`.
 */

/* True when the subject satisfies rule `which`. The subject is whatever the module's check needs to evaluate a rule: an
   exercise, or a small struct of the exercise and the tonic. */
typedef bool (*RuleHolds)(int which, const void *subject);

typedef struct {
    /* The group's name ("harmonic", "cantus-firmus"); NULL for a table that only counts and classifies. */
    const char *name;
    const char *const *names;
    /* One entry per rule, true for a soft rule: failing it never makes a subject invalid. */
    const bool *soft;
    int count;
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

#endif
