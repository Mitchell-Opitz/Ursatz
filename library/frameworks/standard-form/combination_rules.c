#include "combination_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_COMBINATION_RULE_COUNT] = {
    "combination-repeated-phrase",
    "combination-written-out",
    "combination-compound-period",
    "combination-pair-form"};

static StandardFormRulePair g_pairs[STANDARD_FORM_COMBINATION_RULE_COUNT];
static bool g_built[STANDARD_FORM_COMBINATION_RULE_COUNT];

static bool valid_rule(StandardFormCombinationRule which) {
    return which >= 0 && which < STANDARD_FORM_COMBINATION_RULE_COUNT;
}

static void ensure_built(StandardFormCombinationRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_combination_rule(StandardFormCombinationRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_combination_constraint(StandardFormCombinationRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_form_is_repeated_phrase(bool written_out_repeat, CadenceType first, CadenceType second) {
    return written_out_repeat && cadence_type_valid(first) && first == second;
}

bool standard_form_repeat_is_written_out(bool adds_measures) {
    return adds_measures;
}

bool standard_form_is_compound_period(CadenceType antecedent, CadenceType consequent, PhraseLevelForm first_form,
                                      PhraseLevelForm second_form) {
    return standard_form_period_cadences_valid(antecedent, consequent) && first_form == PHRASE_LEVEL_FORM_SENTENCE &&
           second_form == PHRASE_LEVEL_FORM_SENTENCE;
}

bool standard_form_classify_phrase_pair(CadenceType first, CadenceType second, bool second_repeats_first,
                                        PhraseLevelForm first_form, PhraseLevelForm second_form,
                                        PhraseLevelForm *out) {
    if (standard_form_is_repeated_phrase(second_repeats_first, first, second)) {
        *out = PHRASE_LEVEL_FORM_REPEATED_PHRASE;
        return true;
    }
    if (standard_form_is_compound_period(first, second, first_form, second_form)) {
        *out = PHRASE_LEVEL_FORM_COMPOUND_PERIOD;
        return true;
    }
    if (standard_form_period_cadences_valid(first, second)) {
        *out = PHRASE_LEVEL_FORM_PERIOD;
        return true;
    }
    return false;
}
