#include "harmony_hearing_rules.h"

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_HEARING_RULE_COUNT] = {"potential-cadence", "beginning-follows",
                                                              "not-subverted", "phrase-length"};

static bool table_is_hard(int which) {
    return which <= HARMONY_HEARING_NOT_SUBVERTED;
}

static HarmonyRulePair g_pairs[HARMONY_HEARING_RULE_COUNT];
static UrsatzOnce g_built[HARMONY_HEARING_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"hearing", NAMES, HARMONY_HEARING_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate, score_candidate};

const Rule *harmony_hearing_rule(HarmonyHearingRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_hearing_constraint(HarmonyHearingRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_hearing_preference(HarmonyHearingRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_hearing_rule_name(HarmonyHearingRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_hearing_valid(const HarmonyCadenceHearing *hearing) {
    return hearing != NULL && harmony_cadence_valid(&hearing->candidate) && hearing->phrase_measures >= 1;
}

bool harmony_hearing_rule_holds(HarmonyHearingRule which, const HarmonyCadenceHearing *hearing) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_hearing_valid(hearing)) {
        return false;
    }
    switch (which) {
        case HARMONY_HEARING_POTENTIAL_CADENCE:
            return harmony_cadence_rule_holds(HARMONY_CADENCE_CADENTIAL_PROGRESSION, &hearing->candidate);
        case HARMONY_HEARING_BEGINNING_FOLLOWS:
            return hearing->beginning_follows;
        case HARMONY_HEARING_NOT_SUBVERTED:
            return !hearing->candidate.subverted;
        case HARMONY_HEARING_PHRASE_LENGTH:
            return hearing->phrase_measures > 2 || hearing->slow_tempo;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_hearing_rule_holds((HarmonyHearingRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyCadenceHearing)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyCadenceHearing)

bool harmony_hearing_check(const HarmonyCadenceHearing *hearing, unsigned *hard_violations,
                           unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_hearing_valid(hearing)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, hearing, hard_violations, soft_violations);
    return true;
}
