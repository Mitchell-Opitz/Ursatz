#include "phrase_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_PHRASE_RULE_COUNT] = {
    "phrase-complete",
    "phrase-closure"};

static StandardFormRulePair g_pairs[STANDARD_FORM_PHRASE_RULE_COUNT];
static bool g_built[STANDARD_FORM_PHRASE_RULE_COUNT];

static bool valid_rule(StandardFormPhraseRule which) {
    return which >= 0 && which < STANDARD_FORM_PHRASE_RULE_COUNT;
}

static void ensure_built(StandardFormPhraseRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_phrase_rule(StandardFormPhraseRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_phrase_constraint(StandardFormPhraseRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_form_phrase_complete(bool has_beginning, bool has_middle, bool has_end) {
    return has_beginning && has_middle && has_end;
}

bool standard_form_cadence_signals_closure(bool has_cadence) {
    return has_cadence;
}
