#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every counterpoint rule is backed by one Rule and one Constraint
 * referencing standard_counterpoint_framework(). A rule that states what
 * a line or interval pair must be is hard; a rule written as "limit",
 * "avoid", or "prefer" is soft, so failing it never makes a line
 * invalid. constraint_evaluate is never called here: it stays reserved
 * for real L8 generation-pipeline work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} StandardCounterpointRulePair;

/*
 * Builds the pair with ids "standard-counterpoint-rule-<name>" and
 * "standard-counterpoint-constraint-<name>"; the constraint has the given
 * type (never WEIGHTED: no weight is supplied). Returns false (leaving
 * *out untouched) on allocation failure or a WEIGHTED type. The caller
 * owns nothing it must free separately -- callers cache the pair for
 * process lifetime.
 */
bool standard_counterpoint_build_rule_pair(const char *name, ConstraintType type, StandardCounterpointRulePair *out);

#endif
