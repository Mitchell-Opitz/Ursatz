#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "rules/rule_pair.h"
#include "framework.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every markings rule is backed by one Rule and one hard Constraint
 * (a marking either follows the convention or does not -- no
 * weighting) referencing standard_markings_framework(), same shape as
 * common-practice-minimal's chord vocabulary. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef RulePair StandardMarkingsRulePair;

/*
 * Builds the pair with ids "standard-markings-rule-<name>" and
 * "standard-markings-constraint-<name>". Returns false (leaving *out
 * untouched) on allocation failure. The caller owns nothing it must
 * free separately -- callers cache the pair for process lifetime, as
 * chord_vocabulary does.
 */
static inline bool standard_markings_build_rule_pair(const char *name, StandardMarkingsRulePair *out) {
    return rule_pair_build(standard_markings_framework(), "standard-markings", name, out);
}

#endif
