#include "harmony_phrase_rules.h"

#include "harmony_functions.h"
#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_PHRASE_RULE_COUNT] = {"tonic-then-dominant", "no-backward-motion",
                                                             "weak-predominant-leads-on", "iv-before-ii"};

static bool table_is_hard(int which) {
    return which == HARMONY_PHRASE_TONIC_THEN_DOMINANT || which == HARMONY_PHRASE_IV_BEFORE_II;
}

static HarmonyRulePair g_pairs[HARMONY_PHRASE_RULE_COUNT];
static bool g_built[HARMONY_PHRASE_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"phrase", NAMES, HARMONY_PHRASE_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *harmony_phrase_rule(HarmonyPhraseRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_phrase_constraint(HarmonyPhraseRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_phrase_rule_name(HarmonyPhraseRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_phrase_valid(const HarmonyPhrase *phrase) {
    if (phrase == NULL || phrase->chords == NULL || phrase->chord_count == 0 || !harmony_mode_valid(phrase->mode)) {
        return false;
    }
    for (size_t i = 0; i < phrase->chord_count; i++) {
        if (!harmony_chord_valid(&phrase->chords[i])) {
            return false;
        }
    }
    return true;
}

static HarmonicFunction function_at(const HarmonyPhrase *phrase, size_t i) {
    HarmonicFunction function = HARMONIC_FUNCTION_TONIC;
    harmony_function_of(phrase->mode, &phrase->chords[i], &function);
    return function;
}

static bool check_tonic_then_dominant(const HarmonyPhrase *phrase) {
    bool seen_tonic = false;
    for (size_t i = 0; i < phrase->chord_count; i++) {
        HarmonicFunction function = function_at(phrase, i);
        if (function == HARMONIC_FUNCTION_TONIC) {
            seen_tonic = true;
        } else if (function == HARMONIC_FUNCTION_DOMINANT && seen_tonic) {
            return true;
        }
    }
    return false;
}

static bool check_no_backward_motion(const HarmonyPhrase *phrase) {
    for (size_t i = 0; i + 1 < phrase->chord_count; i++) {
        HarmonicFunction from = function_at(phrase, i);
        HarmonicFunction to = function_at(phrase, i + 1);
        if ((from == HARMONIC_FUNCTION_DOMINANT && to == HARMONIC_FUNCTION_PREDOMINANT) ||
            (from == HARMONIC_FUNCTION_PREDOMINANT && to == HARMONIC_FUNCTION_TONIC)) {
            return false;
        }
    }
    return true;
}

static bool check_weak_predominant_leads_on(const HarmonyPhrase *phrase) {
    for (size_t i = 0; i + 1 < phrase->chord_count; i++) {
        HarmonyPredominantStrength strength;
        if (harmony_predominant_strength(phrase->mode, &phrase->chords[i], &strength) &&
            strength == HARMONY_PREDOMINANT_WEAK && function_at(phrase, i + 1) != HARMONIC_FUNCTION_PREDOMINANT) {
            return false;
        }
    }
    return true;
}

/* A ii (degree 2) never comes before a IV (degree 4) in one run of predominants. */
static bool check_iv_before_ii(const HarmonyPhrase *phrase) {
    bool seen_ii = false;
    for (size_t i = 0; i < phrase->chord_count; i++) {
        if (function_at(phrase, i) != HARMONIC_FUNCTION_PREDOMINANT) {
            seen_ii = false;
        } else if (phrase->chords[i].degree == 2) {
            seen_ii = true;
        } else if (phrase->chords[i].degree == 4 && seen_ii) {
            return false;
        }
    }
    return true;
}

bool harmony_phrase_rule_holds(HarmonyPhraseRule which, const HarmonyPhrase *phrase) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_phrase_valid(phrase)) {
        return false;
    }
    switch (which) {
        case HARMONY_PHRASE_TONIC_THEN_DOMINANT:
            return check_tonic_then_dominant(phrase);
        case HARMONY_PHRASE_NO_BACKWARD_MOTION:
            return check_no_backward_motion(phrase);
        case HARMONY_PHRASE_WEAK_PREDOMINANT_LEADS_ON:
            return check_weak_predominant_leads_on(phrase);
        case HARMONY_PHRASE_IV_BEFORE_II:
            return check_iv_before_ii(phrase);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_phrase_rule_holds((HarmonyPhraseRule)which, subject);
}

bool harmony_phrase_check(const HarmonyPhrase *phrase, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_phrase_valid(phrase)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, phrase, hard_violations, soft_violations);
    return true;
}
