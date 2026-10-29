#ifndef URSATZ_RULES_RULE_PAIR_H
#define URSATZ_RULES_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "theory/framework.h"

/*
 * Shared construction helper for a framework's Rule/Constraint pairs. Every framework rule is backed by one Rule and one hard
 * Constraint (a value either follows the convention or does not, no weighting) referencing the framework.
 * constraint_evaluate is never called here: it stays reserved for real L8 generation-pipeline work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} RulePair;

/*
 * Builds the pair with ids "<prefix>-rule-<name>" and "<prefix>-constraint-<name>" under `framework`, the constraint hard.
 * Returns false (leaving *out untouched) on a NULL argument or allocation failure. The caller owns the rule and the
 * constraint; frameworks cache the pair for process lifetime, as chord_vocabulary does.
 */
bool rule_pair_build(const Framework *framework, const char *prefix, const char *name, RulePair *out);

#endif
