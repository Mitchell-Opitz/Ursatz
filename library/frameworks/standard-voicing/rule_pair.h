#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_RULE_PAIR_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_RULE_PAIR_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "rules/rule_pair.h"
#include "framework.h"

/*
 * Shared construction helper for this module's Rule/Constraint pairs.
 * Every voicing rule is backed by one Rule and one Constraint referencing
 * standard_voicing_framework(). Almost all are hard (a voicing either
 * follows the convention or does not); the one soft constraint is the
 * preference that a triad double its bass note (doubling_rules.h).
 * constraint_evaluate is never called here: it stays reserved for real L8
 * generation-pipeline work (docs/modules/rules.md).
 */

typedef RulePair StandardVoicingRulePair;

/*
 * Builds the pair with ids "standard-voicing-rule-<name>" and
 * "standard-voicing-constraint-<name>"; the constraint has the given
 * type (never WEIGHTED: no weight is supplied). Returns false (leaving
 * *out untouched) on allocation failure or a WEIGHTED type. The caller
 * owns nothing it must free separately -- callers cache the pair for
 * process lifetime.
 */
static inline bool standard_voicing_build_rule_pair(const char *name, ConstraintType type, StandardVoicingRulePair *out) {
    return rule_pair_build_typed(standard_voicing_framework(), "standard-voicing", name, type, out);
}

#endif
