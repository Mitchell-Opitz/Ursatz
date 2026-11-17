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
 * table holds those parts so a module states only what is its own: its names, its hardness, and how one rule is evaluated.
 *
 * A module defines its table at file scope, with static storage for the cache:
 *
 *   static RulePair g_pairs[N];
 *   static UrsatzOnce g_built[N];
 *   static const RuleTable TABLE = {"standard-counterpoint", standard_counterpoint_framework, "standard-counterpoint", "prefix",
 *                                   NAMES, N, is_hard, g_pairs, g_built, NULL, NULL};
 *
 * A pair is built the first time it is asked for, with the ids "<id_prefix>-rule-<prefix>-<name>" and the like (the bare name
 * when the prefix is NULL), and cached for the life of the process. Rules are numbered 0 to count - 1, the values of the
 * module's enum; every function returns false or NULL for a number out of range.
 *
 * A table that only counts and classifies its rules (galant, scale harmonization: they use valid, is_hard and check) sets only
 * `count` and `is_hard` and leaves the rest NULL.
 *
 * Evaluation: a table may carry an evaluate callback, registered on each of its Constraints when the pair is built
 * (constraint_set_evaluate_fn). A table with none (NULL) leaves its Constraints unregistered. The callback is normally defined
 * with RULE_TABLE_EVALUATOR below and calls rule_table_evaluate.
 *
 * Scoring works the same way for the table's soft rules: a table may carry a score callback, registered on a Preference built
 * for each soft rule (rule_table_preference; NULL for a hard rule or a table with no callback). It is normally defined with
 * RULE_TABLE_SCORER and calls rule_table_score.
 */

/* True when rule `which` is hard. */
typedef bool (*RuleHard)(int which);

/* True when the subject satisfies rule `which`. The subject is whatever the module's check needs to evaluate a rule: an
   exercise, or a small struct of the exercise and the tonic. */
typedef bool (*RuleHolds)(int which, const void *subject);

/* True when the subject is one rule `which` has anything to say about. NULL means every subject applies. */
typedef bool (*RuleApplies)(int which, const void *subject);

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
    /* NULL when the table registers no evaluation callback. */
    ConstraintEvaluateFn evaluate;
    /* NULL when the table registers no scoring callback. */
    PreferenceScoreFn score;
} RuleTable;

/* True for a rule number from 0 to count - 1. */
bool rule_table_valid(const RuleTable *table, int which);

/* True when the rule is hard; false when it is soft or the number is invalid. */
bool rule_table_is_hard(const RuleTable *table, int which);

/* The soft rule's Preference; NULL for a hard rule, a table with no score callback, or an invalid number. */
const Preference *rule_table_preference(const RuleTable *table, int which);
const char *rule_table_name(const RuleTable *table, int which);

/*
 * Runs every rule on the subject. Sets bit (1u << rule) in *hard_violations for each hard rule that fails and in
 * *soft_violations for each soft rule that fails. Both outputs are written, and a rule that holds sets no bit.
 */
void rule_table_check(const RuleTable *table, RuleHolds holds, const void *subject, unsigned *hard_violations,
                      unsigned *soft_violations);

/*
 * Writes the type tag of a candidate Value for this table into out: "<framework_name>/<prefix>". Returns false if the table has
 * no framework name or prefix or out is too small.
 */
bool rule_table_candidate_type(const RuleTable *table, char *out, size_t capacity);

/*
 * Evaluates `constraint` (one of the table's own) on `candidate`.
 * - candidate whose type tag is not this table's: NOT_APPLICABLE (a different kind of candidate is not a failure);
 * - right tag but content not exactly subject_size bytes: ERR_NULL_CANDIDATE;
 * - applies (when non-NULL) says the rule has nothing to say about the subject: NOT_APPLICABLE;
 * - otherwise PASS if holds(rule, subject) else FAIL. The subject is the candidate's bytes, borrowed for the call only; it may
 *   itself point at data the caller keeps alive.
 * Returns NOT_IMPLEMENTED when constraint is not one of the table's.
 */
ConstraintEvalResult rule_table_evaluate(const RuleTable *table, RuleHolds holds, RuleApplies applies, size_t subject_size,
                                         const Constraint *constraint, const Value *candidate);

/*
 * Defines the evaluate callback for a table: a ConstraintEvaluateFn named `fn` that evaluates against `table` with `holds`, for
 * candidates whose content is a `subject_type`, and every rule applies. Use after the table and its holds adapter; the table's
 * initializer names `fn`, so declare it first:
 *   static ConstraintEvalResult fn(const Constraint *, const Value *, const Context *);
 * A table with an applies callback defines its own callback and calls rule_table_evaluate.
 */
#define RULE_TABLE_EVALUATOR(fn, table, holds, subject_type)                                                          \
    static ConstraintEvalResult fn(const Constraint *constraint, const Value *candidate, const Context *context) {   \
        (void)context;                                                                                                \
        return rule_table_evaluate(&(table), (holds), NULL, sizeof(subject_type), constraint, candidate);            \
    }

/*
 * Scores `candidate` for `preference` (one of the table's own). Never rejects a candidate: a rule that holds scores 1/1, one
 * that does not scores 0/1. A candidate of another type tag, content not exactly subject_size bytes, or a preference that is
 * not the table's cannot be scored and gets {0, 0} (the callback of a Preference with no score reads as unscored).
 */
PreferenceScore rule_table_score(const RuleTable *table, RuleHolds holds, size_t subject_size, const Preference *preference,
                                 const Value *candidate);

/*
 * Defines the scoring callback for a table: a PreferenceScoreFn named `fn` that scores against `table` with `holds`, for
 * candidates whose content is a `subject_type`. Declare it before the table, as for the evaluate callback:
 *   static PreferenceScore fn(const Preference *, const Value *, const Context *);
 */
#define RULE_TABLE_SCORER(fn, table, holds, subject_type)                                                             \
    static PreferenceScore fn(const Preference *preference, const Value *candidate, const Context *context) {        \
        (void)context;                                                                                                \
        return rule_table_score(&(table), (holds), sizeof(subject_type), preference, candidate);                     \
    }

/* Hardness predicate for a table whose rules are all soft. */
bool rule_table_never_hard(int which);

#endif
