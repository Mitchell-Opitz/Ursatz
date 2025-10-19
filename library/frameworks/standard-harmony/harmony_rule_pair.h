#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/preference.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every rule is backed by one Rule and one Constraint referencing
 * standard_harmony_framework(). A rule that states what the style
 * requires is hard; a rule written as "limit", "avoid", or "prefer" is
 * soft, so failing it never makes a subject invalid. This module never calls
 * constraint_evaluate or preference_rank; it only registers the callbacks a
 * rule table supplies (harmony_rule_table.h) on the Constraint it builds and,
 * for a soft rule, on a Preference it builds beside it.
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
    /* NULL for a hard rule, and for a soft rule whose table registers no scoring callback. */
    Preference *preference;
} HarmonyRulePair;

/*
 * Builds the pair with ids "scale-rule-<name>" and
 * "scale-constraint-<name>"; the constraint has the given type
 * (never WEIGHTED: no weight is supplied). Returns false (leaving *out
 * untouched) on allocation failure or a WEIGHTED type. evaluate is the
 * callback registered on the Constraint (constraint_set_evaluate_fn); NULL
 * leaves it unregistered. A soft rule (type SOFT) with a non-NULL score
 * also gets a Preference, id "standard-harmony-preference-<name>", with that
 * scoring callback registered (preference_set_score_fn); otherwise
 * out->preference is NULL. Callers cache the pair for process lifetime.
 */
bool harmony_build_rule_pair(const char *name, ConstraintType type, ConstraintEvaluateFn evaluate,
                             PreferenceScoreFn score, HarmonyRulePair *out);

#endif
