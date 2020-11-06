#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every tonality rule is backed by one Rule and one hard Constraint
 * (a pitch relationship either holds under the convention or does not -- no
 * weighting) referencing standard_tonality_framework(), same shape as
 * common-practice-minimal's chord vocabulary. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} StandardTonalityRulePair;

/*
 * Builds the pair with ids "standard-tonality-rule-<name>" and
 * "standard-tonality-constraint-<name>". Returns false (leaving *out
 * untouched) on allocation failure. The caller owns nothing it must
 * free separately -- callers cache the pair for process lifetime, as
 * chord_vocabulary does.
 */
bool standard_tonality_build_rule_pair(const char *name, StandardTonalityRulePair *out);

#endif
