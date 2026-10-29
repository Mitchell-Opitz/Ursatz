#ifndef URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_SIXTEENTH_CENTURY_STYLE_CENTURY_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "rules/rule_pair.h"
#include "century_framework.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every rule is backed by one Rule and one Constraint referencing
 * sixteenth_century_framework(). A rule that states what the style
 * requires is hard; a rule written as "limit", "avoid", or "prefer" is
 * soft, so failing it never makes a score invalid. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef RulePair SixteenthCenturyRulePair;

/*
 * Builds the pair with ids "sixteenth-century-rule-<name>" and
 * "sixteenth-century-constraint-<name>"; the constraint has the given type
 * (never WEIGHTED: no weight is supplied). Returns false (leaving *out
 * untouched) on allocation failure or a WEIGHTED type. Callers cache the
 * pair for process lifetime.
 */
static inline bool sixteenth_century_build_rule_pair(const char *name, ConstraintType type, SixteenthCenturyRulePair *out) {
    return rule_pair_build_typed(sixteenth_century_framework(), "sixteenth-century", name, type, out);
}

#endif
