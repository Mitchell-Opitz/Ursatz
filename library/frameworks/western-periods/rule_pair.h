#ifndef URSATZ_FRAMEWORKS_WESTERN_PERIODS_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_WESTERN_PERIODS_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every periods rule is backed by one Rule and one hard Constraint
 * (a period boundary either holds or does not -- no
 * weighting) referencing western_periods_framework(), same shape as
 * common-practice-minimal's chord vocabulary. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} WesternPeriodsRulePair;

/*
 * Builds the pair with ids "western-periods-rule-<name>" and
 * "western-periods-constraint-<name>". Returns false (leaving *out
 * untouched) on allocation failure. The caller owns nothing it must
 * free separately -- callers cache the pair for process lifetime, as
 * chord_vocabulary does.
 */
bool western_periods_build_rule_pair(const char *name, WesternPeriodsRulePair *out);

#endif
