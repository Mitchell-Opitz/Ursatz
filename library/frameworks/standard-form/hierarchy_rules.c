#include "hierarchy_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_HIERARCHY_RULE_COUNT] = {
    "hierarchy-level-order",
    "hierarchy-levels-collapse",
    "hierarchy-phrase-scope"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_HIERARCHY_RULE_COUNT);

static bool valid_rule(StandardFormHierarchyRule which) {
    return which >= 0 && which < STANDARD_FORM_HIERARCHY_RULE_COUNT;
}

const Rule *standard_form_hierarchy_rule(StandardFormHierarchyRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_form_hierarchy_constraint(StandardFormHierarchyRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_form_level_contains(FormLevel outer, FormLevel inner) {
    return form_level_valid(outer) && form_level_valid(inner) && outer < inner;
}

bool standard_form_level_next_smaller(FormLevel level, FormLevel *out) {
    if (!form_level_valid(level) || level + 1 >= FORM_LEVEL_COUNT) {
        return false;
    }
    *out = (FormLevel)(level + 1);
    return true;
}

bool standard_form_levels_may_collapse(FormLevel outer, FormLevel inner) {
    return standard_form_level_contains(outer, inner);
}

bool standard_form_level_is_phrase_level(FormLevel level) {
    return form_level_valid(level) && level >= FORM_LEVEL_PHRASE;
}
