#ifndef URSATZ_RULES_RULE_PAIR_H
#define URSATZ_RULES_RULE_PAIR_H

#include <stdbool.h>

#include <stddef.h>

#include "core/once.h"
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

/* The same with the given constraint type, for the frameworks whose rules are hard or soft. WEIGHTED is refused (no weight is
   supplied): false. */
bool rule_pair_build_typed(const Framework *framework, const char *prefix, const char *name, ConstraintType type,
                           RulePair *out);

/*
 * A framework's table of rules, built once and safe for concurrent first use. Declare it static with
 * RULE_PAIR_TABLE_INIT(framework_accessor, "<prefix>", NAMES, COUNT) (or RULE_PAIR_TABLE_INIT_TYPED when the rules
 * are not hard), where NAMES holds `count` rule names. The first rule_pair_table_get() from any thread builds every
 * pair, ids "<prefix>-rule-<name>" and "<prefix>-constraint-<name>", and caches them for the life of the process.
 */
typedef struct {
    UrsatzOnce once;
    const Framework *(*framework)(void);
    const char *prefix;
    const char *const *names;
    size_t count;
    ConstraintType type;
    RulePair *pairs;
} RulePairTable;

#define RULE_PAIR_TABLE_INIT_TYPED(framework_fn, table_prefix, rule_names, rule_count, constraint_type) \
    {URSATZ_ONCE_INIT, (framework_fn), (table_prefix), (rule_names), (rule_count), (constraint_type), NULL}
#define RULE_PAIR_TABLE_INIT(framework_fn, table_prefix, rule_names, rule_count) \
    RULE_PAIR_TABLE_INIT_TYPED(framework_fn, table_prefix, rule_names, rule_count, CONSTRAINT_TYPE_HARD)

/*
 * The pair for rule number `which`. NULL for an out-of-range index, for a NULL table, and when the pair failed to build
 * (a failed build stays failed for good, as the per-file tables did). The pair is owned by the table.
 */
const RulePair *rule_pair_table_get(RulePairTable *table, size_t which);

#endif
