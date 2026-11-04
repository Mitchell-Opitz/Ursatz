#include "hierarchy_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_HIERARCHY_RULE_COUNT] = {
    "hierarchy-level-order",
    "hierarchy-levels-collapse",
    "hierarchy-phrase-scope"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_HIERARCHY_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_form_hierarchy, StandardFormHierarchyRule, STANDARD_FORM_HIERARCHY_RULE_COUNT, g_table)

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
