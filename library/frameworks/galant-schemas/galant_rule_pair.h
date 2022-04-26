#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every rule is backed by one Rule and one Constraint referencing
 * galant_schemas_framework(). A rule that states what the style
 * requires is hard; a rule written as "limit", "avoid", or "prefer" is
 * soft, so failing it never makes an exemplar invalid. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} GalantRulePair;

/*
 * Builds the pair with ids "galant-rule-<name>" and
 * "galant-constraint-<name>"; the constraint has the given type
 * (never WEIGHTED: no weight is supplied). Returns false (leaving *out
 * untouched) on allocation failure or a WEIGHTED type. Callers cache the
 * pair for process lifetime.
 */
bool galant_build_rule_pair(const char *name, ConstraintType type, GalantRulePair *out);

#endif
