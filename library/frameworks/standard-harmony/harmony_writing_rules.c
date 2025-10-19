#include "harmony_writing_rules.h"

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_WRITING_RULE_COUNT] = {"bass-motion", "soprano-melody",
                                                              "minor-leading-tone", "common-hc-soprano"};

static bool table_is_hard(int which) {
    return which <= HARMONY_WRITING_MINOR_LEADING_TONE;
}

static HarmonyRulePair g_pairs[HARMONY_WRITING_RULE_COUNT];
static bool g_built[HARMONY_WRITING_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"writing", NAMES, HARMONY_WRITING_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate, score_candidate};

const Rule *harmony_writing_rule(HarmonyWritingRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_writing_constraint(HarmonyWritingRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_writing_preference(HarmonyWritingRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_writing_rule_name(HarmonyWritingRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool degree_valid(int degree) {
    return degree >= 1 && degree <= 7;
}

bool harmony_writing_valid(const HarmonyCadenceWriting *writing) {
    return writing != NULL && harmony_mode_valid(writing->mode) && cadence_type_valid(writing->type) &&
           writing->type != CADENCE_TYPE_SUBVERTED && degree_valid(writing->bass[0]) &&
           degree_valid(writing->bass[1]) && degree_valid(writing->soprano[0]) && degree_valid(writing->soprano[1]);
}

static bool soprano_is(const HarmonyCadenceWriting *w, int first, int second) {
    return w->soprano[0] == first && w->soprano[1] == second;
}

static bool check_bass_motion(const HarmonyCadenceWriting *w) {
    if (w->type == CADENCE_TYPE_HALF) {
        return w->bass[0] == 1 && w->bass[1] == 5;
    }
    return w->bass[0] == 5 && w->bass[1] == 1;
}

static bool check_soprano_melody(const HarmonyCadenceWriting *w) {
    switch (w->type) {
        case CADENCE_TYPE_PERFECT_AUTHENTIC:
            return soprano_is(w, 2, 1) || soprano_is(w, 7, 1);
        case CADENCE_TYPE_IMPERFECT_AUTHENTIC:
            return soprano_is(w, 2, 3) || soprano_is(w, 5, 5);
        case CADENCE_TYPE_HALF:
            return soprano_is(w, 1, 7) || soprano_is(w, 3, 2) || soprano_is(w, 5, 5);
        default:
            return false;
    }
}

static bool check_common_hc_soprano(const HarmonyCadenceWriting *w) {
    return w->type != CADENCE_TYPE_HALF || soprano_is(w, 1, 7) || soprano_is(w, 3, 2);
}

bool harmony_writing_rule_holds(HarmonyWritingRule which, const HarmonyCadenceWriting *writing) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_writing_valid(writing)) {
        return false;
    }
    switch (which) {
        case HARMONY_WRITING_BASS_MOTION:
            return check_bass_motion(writing);
        case HARMONY_WRITING_SOPRANO_MELODY:
            return check_soprano_melody(writing);
        case HARMONY_WRITING_MINOR_LEADING_TONE:
            return writing->mode == HARMONY_MODE_MAJOR || writing->dominant_has_leading_tone;
        case HARMONY_WRITING_COMMON_HC_SOPRANO:
            return check_common_hc_soprano(writing);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_writing_rule_holds((HarmonyWritingRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyCadenceWriting)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyCadenceWriting)

bool harmony_writing_check(const HarmonyCadenceWriting *writing, unsigned *hard_violations,
                           unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_writing_valid(writing)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, writing, hard_violations, soft_violations);
    return true;
}
