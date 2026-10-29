#include "ending_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_ENDING_RULE_COUNT] = {
    "ending-cadence",
    "ending-division",
    "ending-fragmentation",
    "ending-stability",
    "ending-classify"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_ENDING_RULE_COUNT);

static bool valid_rule(StandardFormEndingRule which) {
    return which >= 0 && which < STANDARD_FORM_ENDING_RULE_COUNT;
}

const Rule *standard_form_ending_rule(StandardFormEndingRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_form_ending_constraint(StandardFormEndingRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

static bool is_authentic(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
}

bool standard_form_ending_cadence_allowed(PhraseEnding ending, CadenceType type) {
    switch (ending) {
        case PHRASE_ENDING_CONSEQUENT:
        case PHRASE_ENDING_CADENTIAL:
            return is_authentic(type);
        case PHRASE_ENDING_CONTINUATION:
            return standard_form_cadence_is_phrase_goal(type);
        default:
            return false;
    }
}

bool standard_form_ending_division_usual(PhraseEnding ending, const size_t *units, size_t unit_count) {
    if (units == NULL) {
        return false;
    }
    switch (ending) {
        case PHRASE_ENDING_CONSEQUENT:
            return unit_count == 2;
        case PHRASE_ENDING_CONTINUATION:
            return (unit_count == 3 && units[0] == 1 && units[1] == 1 && units[2] == 2) ||
                   (unit_count == 2 && units[0] == 2 && units[1] == 2);
        case PHRASE_ENDING_CADENTIAL:
            return unit_count == 1 || (unit_count == 2 && units[0] == 2 && units[1] == 2);
        default:
            return false;
    }
}

bool standard_form_ending_allows_fragmentation(PhraseEnding ending) {
    return phrase_ending_valid(ending) && ending != PHRASE_ENDING_CADENTIAL;
}

bool standard_form_ending_start_stability(PhraseEnding ending, StandardFormStability *out) {
    switch (ending) {
        case PHRASE_ENDING_CONSEQUENT:
            *out = STANDARD_FORM_STABILITY_STABLE;
            return true;
        case PHRASE_ENDING_CONTINUATION:
            *out = STANDARD_FORM_STABILITY_UNSTABLE;
            return true;
        default:
            return false;
    }
}

bool standard_form_classify_ending(CadenceType cadence, bool bass_is_cadential_pattern,
                                   bool basic_then_contrasting_ideas, bool has_fragmentation,
                                   PhraseEnding *out) {
    if (!standard_form_cadence_is_phrase_goal(cadence)) {
        return false;
    }
    if (bass_is_cadential_pattern && is_authentic(cadence) && !has_fragmentation) {
        *out = PHRASE_ENDING_CADENTIAL;
    } else if (basic_then_contrasting_ideas && is_authentic(cadence)) {
        *out = PHRASE_ENDING_CONSEQUENT;
    } else {
        *out = PHRASE_ENDING_CONTINUATION;
    }
    return true;
}
