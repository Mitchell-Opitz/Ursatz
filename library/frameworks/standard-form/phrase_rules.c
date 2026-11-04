#include "phrase_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_PHRASE_RULE_COUNT] = {
    "phrase-complete",
    "phrase-closure",
    "phrase-length",
    "phrase-goal",
    "phrase-diagram"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_PHRASE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_form_phrase, StandardFormPhraseRule, STANDARD_FORM_PHRASE_RULE_COUNT, g_table)

bool standard_form_phrase_complete(bool has_beginning, bool has_middle, bool has_end) {
    return has_beginning && has_middle && has_end;
}

bool standard_form_cadence_signals_closure(bool has_cadence) {
    return has_cadence;
}

bool standard_form_phrase_length_valid(size_t measures) {
    return measures >= 1;
}

bool standard_form_phrase_length_common(size_t measures) {
    return measures == 4 || measures == 8 || measures == 16;
}

bool standard_form_cadence_is_phrase_goal(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
           type == CADENCE_TYPE_HALF;
}

bool standard_form_level_diagram_mark(FormLevel level, StandardFormDiagramMark *out) {
    if (!form_level_valid(level) || level == FORM_LEVEL_MOTIVE) {
        return false;
    }
    *out = (level == FORM_LEVEL_IDEA || level == FORM_LEVEL_SUBPHRASE) ? STANDARD_FORM_DIAGRAM_MARK_BRACKET
                                                                        : STANDARD_FORM_DIAGRAM_MARK_ARC;
    return true;
}
