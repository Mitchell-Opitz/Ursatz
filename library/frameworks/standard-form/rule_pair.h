#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every form rule is backed by one Rule and one hard Constraint
 * (a value either follows the convention or does not -- no
 * weighting) referencing standard_form_framework(), same shape as
 * common-practice-minimal's chord vocabulary. constraint_evaluate is
 * never called here: it stays reserved for real L8 generation-pipeline
 * work (docs/modules/rules.md).
 */

typedef struct {
    Rule *rule;
    Constraint *constraint;
} StandardFormRulePair;

/*
 * Builds the pair with ids "standard-form-rule-<name>" and
 * "standard-form-constraint-<name>". Returns false (leaving *out
 * untouched) on allocation failure. The caller owns nothing it must
 * free separately -- callers cache the pair for process lifetime, as
 * chord_vocabulary does.
 */
bool standard_form_build_rule_pair(const char *name, StandardFormRulePair *out);

#endif
