#ifndef URSATZ_THEORY_RULES_RULE_PAIR_H
#define URSATZ_THEORY_RULES_RULE_PAIR_H

#include <stdbool.h>

#include "theory/framework/framework.h"
#include "theory/rules/constraint.h"
#include "theory/rules/preference.h"
#include "theory/rules/rule.h"

/*
 * Shared construction helper for the Rule/Constraint pairs of every area's rule table (rule_table.h). Each rule is backed by one
 * Rule and one Constraint referencing the area's framework. A rule that states what a subject must be is hard; a rule written as
 * "limit", "avoid" or "prefer" is soft, so failing it never makes a subject invalid. This module never evaluates a Constraint or
 * ranks a Preference; it only registers the callbacks a rule table supplies on the Constraint it builds and, for a soft rule, on a
 * Preference it builds beside it.
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
    /* NULL for a hard rule, and for a soft rule whose table registers no scoring callback. */
    Preference *preference;
} RulePair;

/*
 * Builds the pair for `framework` with ids "<id_prefix>-rule-<name>" and "<id_prefix>-constraint-<name>"; the constraint has the
 * given type (never WEIGHTED: no weight is supplied). Returns false (leaving *out untouched) on a NULL framework, allocation
 * failure or a WEIGHTED type. evaluate is the callback registered on the Constraint (constraint_set_evaluate_fn); NULL leaves it
 * unregistered. A soft rule (type SOFT) with a non-NULL score also gets a Preference, id "<id_prefix>-preference-<name>", with
 * that scoring callback registered (preference_set_score_fn); otherwise out->preference is NULL. The caller owns nothing it must
 * free separately: callers cache the pair for process lifetime.
 */
bool rule_pair_build(const Framework *framework, const char *id_prefix, const char *name, ConstraintType type,
                     ConstraintEvaluateFn evaluate, PreferenceScoreFn score, RulePair *out);

#endif
