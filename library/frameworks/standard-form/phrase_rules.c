#include "phrase_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_PHRASE_RULE_COUNT] = {
    "phrase-complete",
    "phrase-closure",
    "phrase-length",
    "phrase-goal",
    "phrase-diagram"};

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

int standard_form_cadence_strength(CadenceType type) {
    switch (type) {
        case CADENCE_TYPE_HALF:
            return 1;
        case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
            return 2;
        case CADENCE_TYPE_PERFECT_AUTHENTIC:
            return 3;
        default:
            return 0;
    }
}

bool standard_form_level_diagram_mark(FormLevel level, StandardFormDiagramMark *out) {
    if (!form_level_valid(level) || level == FORM_LEVEL_MOTIVE) {
        return false;
    }
    *out = (level == FORM_LEVEL_IDEA || level == FORM_LEVEL_SUBPHRASE) ? STANDARD_FORM_DIAGRAM_MARK_BRACKET
                                                                        : STANDARD_FORM_DIAGRAM_MARK_ARC;
    return true;
}
