#include "theory/form/beginning_rules.h"

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
