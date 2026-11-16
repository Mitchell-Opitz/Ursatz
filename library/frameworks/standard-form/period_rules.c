#include "period_rules.h"

#include "standard-harmony/harmony_cadence_rules.h"

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

bool standard_form_consequent_contrast_usual(bool contrasting_idea_differs) {
    return contrasting_idea_differs;
}

