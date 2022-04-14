#ifndef URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_BAROQUE_GROUND_BASS_GROUND_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every rule is backed by one Rule and one Constraint referencing
 * baroque_ground_bass_framework(). A rule that states what the style
 * requires is hard; a rule written as "limit", "avoid", or "prefer" is
 * soft, so failing it never makes a piece invalid. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} GroundRulePair;

/*
 * Builds the pair with ids "ground-rule-<name>" and
 * "ground-constraint-<name>"; the constraint has the given type
 * (never WEIGHTED: no weight is supplied). Returns false (leaving *out
 * untouched) on allocation failure or a WEIGHTED type. Callers cache the
 * pair for process lifetime.
 */
bool ground_build_rule_pair(const char *name, ConstraintType type, GroundRulePair *out);

#endif
