#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "rules/rule_pair.h"
#include "framework.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every notation rule is backed by one Rule and one hard Constraint
 * (a placement either follows the convention or does not -- no
 * weighting) referencing standard_notation_framework(), same shape as
 * common-practice-minimal's chord vocabulary. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef RulePair StandardNotationRulePair;

/*
 * Builds the pair with ids "standard-notation-rule-<name>" and
 * "standard-notation-constraint-<name>". Returns false (leaving *out
 * untouched) on allocation failure. The caller owns nothing it must
 * free separately -- callers cache the pair for process lifetime, as
 * chord_vocabulary does.
 */
static inline bool standard_notation_build_rule_pair(const char *name, StandardNotationRulePair *out) {
    return rule_pair_build(standard_notation_framework(), "standard-notation", name, out);
}

#endif
