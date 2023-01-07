#include "hybrid_rules.h"

#include "rule_pair.h"
#include <stddef.h>

static const char *const NAMES[STANDARD_FORM_HYBRID_RULE_COUNT] = {
    "hybrid-pairing-form",
    "hybrid-commonality",
    "hybrid-subphrase-role"};

static StandardFormRulePair g_pairs[STANDARD_FORM_HYBRID_RULE_COUNT];
static bool g_built[STANDARD_FORM_HYBRID_RULE_COUNT];

static bool valid_rule(StandardFormHybridRule which) {
    return which >= 0 && which < STANDARD_FORM_HYBRID_RULE_COUNT;
}

static void ensure_built(StandardFormHybridRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_form_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_form_hybrid_rule(StandardFormHybridRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_form_hybrid_constraint(StandardFormHybridRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

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

bool standard_form_beginning_of_subphrase_role(SubphraseRole role, PhraseBeginning *out) {
    if (role != SUBPHRASE_ROLE_PRESENTATION) {
        return false;
    }
    *out = PHRASE_BEGINNING_PRESENTATION;
    return true;
}

bool standard_form_ending_of_subphrase_role(SubphraseRole role, PhraseEnding *out) {
    if (role != SUBPHRASE_ROLE_CONTINUATION) {
        return false;
    }
    *out = PHRASE_ENDING_CONTINUATION;
    return true;
}
