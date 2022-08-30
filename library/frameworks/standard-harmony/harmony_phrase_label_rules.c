#include "harmony_phrase_label_rules.h"

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_LABELS_RULE_COUNT] = {"ends-as-cadence", "starts-on-tonic",
                                                             "has-strong-predominant"};

static bool table_is_hard(int which) {
    return which == HARMONY_LABELS_ENDS_AS_CADENCE;
}

static HarmonyRulePair g_pairs[HARMONY_LABELS_RULE_COUNT];
static bool g_built[HARMONY_LABELS_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"labels", NAMES, HARMONY_LABELS_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *harmony_labels_rule(HarmonyLabelsRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_labels_constraint(HarmonyLabelsRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_labels_rule_name(HarmonyLabelsRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_heard_phrase_valid(const HarmonyHeardPhrase *heard) {
    return heard != NULL && harmony_phrase_valid(&heard->phrase) && cadence_type_valid(heard->cadence);
}

bool harmony_labels_rule_holds(HarmonyLabelsRule which, const HarmonyHeardPhrase *heard) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_heard_phrase_valid(heard)) {
        return false;
    }
    HarmonyPhraseLabels labels;
    bool labeled = harmony_phrase_model_label(&heard->phrase, heard->cadence, &labels);
    switch (which) {
        case HARMONY_LABELS_ENDS_AS_CADENCE:
            return labeled;
        case HARMONY_LABELS_STARTS_ON_TONIC:
            return labeled && harmony_phrase_labels_index_of(&labels, PHRASE_MODEL_LABEL_TONIC_BEGINNING) == 0;
        case HARMONY_LABELS_HAS_STRONG_PREDOMINANT:
            return labeled && harmony_phrase_labels_index_of(&labels, PHRASE_MODEL_LABEL_PREDOMINANT) >= 0;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_labels_rule_holds((HarmonyLabelsRule)which, subject);
}

bool harmony_labels_check(const HarmonyHeardPhrase *heard, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_heard_phrase_valid(heard)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, heard, hard_violations, soft_violations);
    return true;
}
