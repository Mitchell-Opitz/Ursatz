#include "period_rules.h"

#include "standard-harmony/harmony_cadence_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_PERIOD_RULE_COUNT] = {
    "period-structure",
    "period-phrase-ideas",
    "period-cadences",
    "period-antecedent-cadence",
    "period-consequent-cadence",
    "period-length",
    "period-modulation",
    "period-consequent-ideas"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_PERIOD_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_form_period, StandardFormPeriodRule, STANDARD_FORM_PERIOD_RULE_COUNT, g_table)

bool standard_form_phrase_roles_in_order(PhraseRole first, PhraseRole second) {
    return first == PHRASE_ROLE_ANTECEDENT && second == PHRASE_ROLE_CONSEQUENT;
}

bool standard_form_period_phrase_ideas_valid(IdeaLabel first, IdeaLabel second) {
    return first == IDEA_LABEL_BASIC_IDEA && second == IDEA_LABEL_CONTRASTING_IDEA;
}

bool standard_form_period_cadences_valid(CadenceType antecedent, CadenceType consequent) {
    return standard_form_cadence_is_phrase_goal(antecedent) && standard_form_cadence_is_phrase_goal(consequent) &&
           harmony_cadence_compare(consequent, antecedent) > 0;
}

bool standard_form_antecedent_cadence_usual(CadenceType type) {
    return type == CADENCE_TYPE_HALF;
}

bool standard_form_consequent_cadence_usual(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC;
}

bool standard_form_antecedent_length_usual(size_t measures) {
    return measures == 4;
}

bool standard_form_consequent_length_usual(size_t antecedent, size_t consequent) {
    return antecedent > 0 && consequent >= antecedent;
}

bool standard_form_phrase_role_usually_modulates(PhraseRole role) {
    return role == PHRASE_ROLE_CONSEQUENT;
}

bool standard_form_consequent_contrast_usual(bool contrasting_idea_differs) {
    return contrasting_idea_differs;
}

bool standard_form_basic_ideas_need_numbering(bool basic_ideas_differ) {
    return basic_ideas_differ;
}
