#include "beginning_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_BEGINNING_RULE_COUNT] = {
    "beginning-cadence",
    "beginning-ideas",
    "beginning-compound-basic-idea",
    "beginning-classify"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_BEGINNING_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_form_beginning, StandardFormBeginningRule, STANDARD_FORM_BEGINNING_RULE_COUNT, g_table)

bool standard_form_beginning_ends_with_cadence(PhraseBeginning beginning) {
    return beginning == PHRASE_BEGINNING_ANTECEDENT;
}

bool standard_form_beginning_ideas(PhraseBeginning beginning, IdeaLabel *first, IdeaLabel *second) {
    switch (beginning) {
        case PHRASE_BEGINNING_ANTECEDENT:
        case PHRASE_BEGINNING_COMPOUND_BASIC_IDEA:
            *first = IDEA_LABEL_BASIC_IDEA;
            *second = IDEA_LABEL_CONTRASTING_IDEA;
            return true;
        case PHRASE_BEGINNING_PRESENTATION:
            *first = IDEA_LABEL_BASIC_IDEA;
            *second = IDEA_LABEL_BASIC_IDEA;
            return true;
        default:
            return false;
    }
}

bool standard_form_beginning_is_cadence_free_antecedent(PhraseBeginning beginning) {
    return beginning == PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
}

bool standard_form_classify_beginning(bool ends_with_cadence, IdeaLabel first, IdeaLabel second,
                                      PhraseBeginning *out) {
    if (first != IDEA_LABEL_BASIC_IDEA) {
        return false;
    }
    if (second == IDEA_LABEL_CONTRASTING_IDEA) {
        *out = ends_with_cadence ? PHRASE_BEGINNING_ANTECEDENT : PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
        return true;
    }
    if (second == IDEA_LABEL_BASIC_IDEA && !ends_with_cadence) {
        *out = PHRASE_BEGINNING_PRESENTATION;
        return true;
    }
    return false;
}
