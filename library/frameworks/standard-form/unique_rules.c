#include "unique_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_UNIQUE_RULE_COUNT] = {
    "unique-form",
    "unique-archetypal",
    "unique-spectrum",
    "unique-labels"};

static StandardFormRulePair g_pairs[STANDARD_FORM_UNIQUE_RULE_COUNT];
static bool g_built[STANDARD_FORM_UNIQUE_RULE_COUNT];

static bool valid_rule(StandardFormUniqueRule which) {
    return which >= 0 && which < STANDARD_FORM_UNIQUE_RULE_COUNT;
}

static void ensure_built(StandardFormUniqueRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_unique_rule(StandardFormUniqueRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_unique_constraint(StandardFormUniqueRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_form_organized_as_unique(bool organized_as_sentence, bool organized_as_period) {
    return !organized_as_sentence && !organized_as_period;
}

bool standard_form_form_is_archetypal(PhraseLevelForm form) {
    return form == PHRASE_LEVEL_FORM_SENTENCE || form == PHRASE_LEVEL_FORM_PERIOD ||
           form == PHRASE_LEVEL_FORM_COMPOUND_PERIOD;
}

bool standard_form_form_is_unique(PhraseLevelForm form) {
    return form == PHRASE_LEVEL_FORM_UNIQUE;
}

bool standard_form_form_is_mixed(bool shows_archetype_traits, bool shows_non_archetype_traits) {
    return shows_archetype_traits && shows_non_archetype_traits;
}

bool standard_form_label_should_apply(bool supports_interpretation) {
    return supports_interpretation;
}
