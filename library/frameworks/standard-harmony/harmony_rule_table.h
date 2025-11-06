#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RULE_TABLE_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RULE_TABLE_H

#include <stdbool.h>
#include <stddef.h>

#include "harmony_rule_pair.h"
#include "core/value.h"
#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Shared table for a group of rules, the same pattern as
 * standard-counterpoint's rule_table.h: the rule names, which rules are hard
 * and which soft, one cached Rule/Constraint pair for each rule
 * (harmony_rule_pair.h), and a check that runs the whole group and reports
 * the rules a subject breaks as hard and soft bit masks.
 *
 * A module defines its table at file scope, with static storage for the
 * cache:
 *
 *   static HarmonyRulePair g_pairs[N];
 *   static bool g_built[N];
 *   static const HarmonyRuleTable TABLE = {"prefix", NAMES, N, is_hard, g_pairs, g_built};
 *
 * A pair is built the first time it is asked for, with the id
 * "<prefix>-<name>", and cached for the life of the process. Rules are
 * numbered 0 to count - 1, the values of the module's enum; every function
 * returns false or NULL for a number out of range.
 *
 * Evaluation through constraint_evaluate: a table may carry an evaluate
 * callback, registered on each of its Constraints when the pair is built. A
 * table with none (NULL) leaves its Constraints unregistered, so
 * constraint_evaluate reports NOT_IMPLEMENTED for them. The callback is
 * normally defined with HARMONY_RULE_TABLE_EVALUATOR below and calls
 * harmony_rule_table_evaluate.
 *
 * Scoring through preference_rank works the same way for the table's soft
 * rules: a table may carry a score callback, registered on a Preference built
 * for each soft rule (harmony_rule_table_preference; NULL for a hard rule or a
 * table with no callback). It is normally defined with
 * HARMONY_RULE_TABLE_SCORER and calls harmony_rule_table_score.
 */

/* True when rule `which` is hard. */
typedef bool (*HarmonyRuleHard)(int which);

/* True when the subject satisfies rule `which`. */
typedef bool (*HarmonyRuleHolds)(int which, const void *subject);

typedef struct {
    const char *prefix;
    const char *const *names;
    int count;
    /* NULL when every rule is hard. */
    HarmonyRuleHard is_hard;
    HarmonyRulePair *pairs;
    bool *built;
    /* NULL when the table registers no evaluation callback. */
    ConstraintEvaluateFn evaluate;
    /* NULL when the table registers no scoring callback. */
    PreferenceScoreFn score;
} HarmonyRuleTable;

/* True for a rule number from 0 to count - 1. */
bool harmony_rule_table_valid(const HarmonyRuleTable *table, int which);

/* True when the rule is hard; false when it is soft or the number is invalid. */
bool harmony_rule_table_is_hard(const HarmonyRuleTable *table, int which);

/* The rule, its constraint, and its name. NULL for an invalid number. */
const Rule *harmony_rule_table_rule(const HarmonyRuleTable *table, int which);
const Constraint *harmony_rule_table_constraint(const HarmonyRuleTable *table, int which);
/* The soft rule's Preference; NULL for a hard rule, a table with no score callback, or an invalid number. */
const Preference *harmony_rule_table_preference(const HarmonyRuleTable *table, int which);
const char *harmony_rule_table_name(const HarmonyRuleTable *table, int which);

/*
 * Runs every rule on the subject. Sets bit (1u << rule) in *hard_violations
 * for each hard rule that fails and in *soft_violations for each soft rule
 * that fails. Both outputs are written, and a rule that holds sets no bit.
 */
void harmony_rule_table_check(const HarmonyRuleTable *table, HarmonyRuleHolds holds,
                                        const void *subject, unsigned *hard_violations,
                                        unsigned *soft_violations);

/* True when the subject is one rule `which` has anything to say about. NULL
   means every subject applies. */
typedef bool (*HarmonyRuleApplies)(int which, const void *subject);

/*
 * Writes the type tag of a candidate Value for this table into out:
 * "standard-harmony/<prefix>". Returns false if the table has no prefix or
 * out is too small.
 */
bool harmony_rule_table_candidate_type(const HarmonyRuleTable *table, char *out, size_t capacity);

/*
 * Evaluates `constraint` (one of the table's own) on `candidate`.
 * - candidate whose type tag is not this table's: NOT_APPLICABLE (a different
 *   kind of candidate is not a failure);
 * - right tag but content not exactly subject_size bytes: ERR_NULL_CANDIDATE;
 * - applies (when non-NULL) says the rule has nothing to say about the
 *   subject: NOT_APPLICABLE;
 * - otherwise PASS if holds(rule, subject) else FAIL. The subject is the
 *   candidate's bytes, borrowed for the call only; it may itself point at
 *   data the caller keeps alive.
 * Returns NOT_IMPLEMENTED when constraint is not one of the table's.
 */
ConstraintEvalResult harmony_rule_table_evaluate(const HarmonyRuleTable *table, HarmonyRuleHolds holds,
                                                 HarmonyRuleApplies applies, size_t subject_size,
                                                 const Constraint *constraint, const Value *candidate);

/*
 * Defines the evaluate callback for a table: a ConstraintEvaluateFn named
 * `fn` that evaluates against `table` with `holds`, for candidates whose
 * content is a `subject_type`. Use after the table and its holds adapter; the
 * table's initializer names `fn`, so declare it first:
 *   static ConstraintEvalResult fn(const Constraint *, const Value *, const Context *);
 */
#define HARMONY_RULE_TABLE_EVALUATOR(fn, table, holds, subject_type)                                           \
    static ConstraintEvalResult fn(const Constraint *constraint, const Value *candidate,                       \
                                   const Context *context) {                                                    \
        (void)context;                                                                                          \
        return harmony_rule_table_evaluate(&(table), (holds), NULL, sizeof(subject_type), constraint,         \
                                           candidate);                                                          \
    }

/*
 * Scores `candidate` for `preference` (one of the table's own). Never rejects
 * a candidate: a rule that holds scores 1/1, one that does not scores 0/1.
 * A candidate of another type tag, content not exactly subject_size bytes, or
 * a preference that is not the table's cannot be scored and gets {0, 0}
 * (preference_rank reports that as NOT_IMPLEMENTED).
 */
PreferenceScore harmony_rule_table_score(const HarmonyRuleTable *table, HarmonyRuleHolds holds, size_t subject_size,
                                         const Preference *preference, const Value *candidate);

/*
 * Defines the scoring callback for a table: a PreferenceScoreFn named `fn`
 * that scores against `table` with `holds`, for candidates whose content is a
 * `subject_type`. Declare it before the table, as for the evaluate callback:
 *   static PreferenceScore fn(const Preference *, const Value *, const Context *);
 */
#define HARMONY_RULE_TABLE_SCORER(fn, table, holds, subject_type)                                              \
    static PreferenceScore fn(const Preference *preference, const Value *candidate, const Context *context) { \
        (void)context;                                                                                          \
        return harmony_rule_table_score(&(table), (holds), sizeof(subject_type), preference, candidate);       \
    }

/* Hardness predicate for a table whose rules are all soft. */
bool harmony_rule_table_never_hard(int which);

#endif
