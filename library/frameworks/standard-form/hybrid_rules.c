#include "hybrid_rules.h"

#include <stddef.h>

static const char *const COMMONALITY_NAMES[STANDARD_FORM_COMMONALITY_COUNT] = {
    "very-common", "slightly-less-common", "occasional", "rare"};

bool standard_form_commonality_valid(StandardFormCommonality commonality) {
    return commonality >= 0 && commonality < STANDARD_FORM_COMMONALITY_COUNT;
}

const char *standard_form_commonality_name(StandardFormCommonality commonality) {
    return standard_form_commonality_valid(commonality) ? COMMONALITY_NAMES[commonality] : NULL;
}

/* rows are beginnings, columns are endings, in the enum order */
static const StandardFormCommonality COMMONALITY[PHRASE_BEGINNING_COUNT][PHRASE_ENDING_COUNT] = {
    /* antecedent          */ {STANDARD_FORM_COMMONALITY_VERY_COMMON, STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON,
                               STANDARD_FORM_COMMONALITY_OCCASIONAL},
    /* compound basic idea */ {STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON,
                               STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON, STANDARD_FORM_COMMONALITY_OCCASIONAL},
    /* presentation        */ {STANDARD_FORM_COMMONALITY_RARE, STANDARD_FORM_COMMONALITY_VERY_COMMON,
                               STANDARD_FORM_COMMONALITY_RARE},
};

static bool pairing_valid(PhraseBeginning beginning, PhraseEnding ending) {
    return phrase_beginning_valid(beginning) && phrase_ending_valid(ending);
}

bool standard_form_pairing_form(PhraseBeginning beginning, PhraseEnding ending, PhraseLevelForm *out) {
    if (!pairing_valid(beginning, ending)) {
        return false;
    }
    if (beginning == PHRASE_BEGINNING_PRESENTATION && ending == PHRASE_ENDING_CONTINUATION) {
        *out = PHRASE_LEVEL_FORM_SENTENCE;
    } else if (beginning == PHRASE_BEGINNING_ANTECEDENT && ending == PHRASE_ENDING_CONSEQUENT) {
        *out = PHRASE_LEVEL_FORM_PERIOD;
    } else {
        *out = PHRASE_LEVEL_FORM_HYBRID;
    }
    return true;
}

bool standard_form_pairing_is_hybrid(PhraseBeginning beginning, PhraseEnding ending) {
    PhraseLevelForm form;
    return standard_form_pairing_form(beginning, ending, &form) && form == PHRASE_LEVEL_FORM_HYBRID;
}

bool standard_form_pairing_commonality(PhraseBeginning beginning, PhraseEnding ending,
                                       StandardFormCommonality *out) {
    if (!pairing_valid(beginning, ending)) {
        return false;
    }
    *out = COMMONALITY[beginning][ending];
    return true;
}

