#include "hierarchy_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_HIERARCHY_RULE_COUNT] = {
    "hierarchy-level-order",
    "hierarchy-levels-collapse",
    "hierarchy-phrase-scope"};

static StandardFormRulePair g_pairs[STANDARD_FORM_HIERARCHY_RULE_COUNT];
static bool g_built[STANDARD_FORM_HIERARCHY_RULE_COUNT];

static bool valid_rule(StandardFormHierarchyRule which) {
    return which >= 0 && which < STANDARD_FORM_HIERARCHY_RULE_COUNT;
}

static void ensure_built(StandardFormHierarchyRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_hierarchy_rule(StandardFormHierarchyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_hierarchy_constraint(StandardFormHierarchyRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
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
