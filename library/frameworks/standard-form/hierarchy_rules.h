#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_HIERARCHY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_HIERARCHY_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/form_level.h"

/*
 * Hierarchy rules for standard-form-v1. Musical form is a hierarchical grouping of units: a
 * piece holds movements, movements hold sections, sections hold themes, themes hold
 * phrases, and so on down to motives (FormLevel, semantics/form_level.h):
 *
 *   LEVEL_ORDER       each level contains the levels after it in the FormLevel order, and
 *                     no level contains an equal or larger one
 *   LEVELS_COLLAPSE   two levels may be collapsed into one: a single phrase may be a whole
 *                     section, so the hierarchy is a guide, not a strict definition
 *   PHRASE_SCOPE      phrase-level form is how a phrase is built from motives, ideas, and
 *                     sometimes subphrases
 */

typedef enum {
    STANDARD_FORM_HIERARCHY_RULE_LEVEL_ORDER = 0,
    STANDARD_FORM_HIERARCHY_RULE_LEVELS_COLLAPSE,
    STANDARD_FORM_HIERARCHY_RULE_PHRASE_SCOPE,
    STANDARD_FORM_HIERARCHY_RULE_COUNT
} StandardFormHierarchyRule;

const Rule *standard_form_hierarchy_rule(StandardFormHierarchyRule which);
const Constraint *standard_form_hierarchy_constraint(StandardFormHierarchyRule which);

/* LEVEL_ORDER: true when both levels are valid and outer is a larger level than inner. */
bool standard_form_level_contains(FormLevel outer, FormLevel inner);

/* LEVEL_ORDER: the next smaller level after level. False, leaving *out untouched, for an
   invalid level or for the smallest level (motive). */
bool standard_form_level_next_smaller(FormLevel level, FormLevel *out);

/* LEVELS_COLLAPSE: true when a unit at the inner level may fill the whole of the outer
   level, which is whenever the outer level is the larger. */
bool standard_form_levels_may_collapse(FormLevel outer, FormLevel inner);

/* PHRASE_SCOPE: true for phrase, subphrase, idea and motive, the levels phrase-level form is
   made of; false for the larger levels and for an invalid level. */
bool standard_form_level_is_phrase_level(FormLevel level);

#endif
