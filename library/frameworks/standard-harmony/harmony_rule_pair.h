#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every rule is backed by one Rule and one Constraint referencing
 * standard_harmony_framework(). A rule that states what the style
 * requires is hard; a rule written as "limit", "avoid", or "prefer" is
 * soft, so failing it never makes a subject invalid. This module never calls
 * constraint_evaluate; it only registers the evaluation callback a rule table
 * supplies (harmony_rule_table.h) on each Constraint it builds.
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} HarmonyRulePair;

/*
 * Builds the pair with ids "scale-rule-<name>" and
 * "scale-constraint-<name>"; the constraint has the given type
 * (never WEIGHTED: no weight is supplied). Returns false (leaving *out
 * untouched) on allocation failure or a WEIGHTED type. evaluate is the
 * callback registered on the Constraint (constraint_set_evaluate_fn); NULL
 * leaves it unregistered. Callers cache the pair for process lifetime.
 */
bool harmony_build_rule_pair(const char *name, ConstraintType type, ConstraintEvaluateFn evaluate,
                             HarmonyRulePair *out);

#endif
