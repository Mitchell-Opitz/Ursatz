#include "harmony_arpeggiating_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_ARPEGGIATING_RULE_COUNT] = {"is-six-four", "same-chord", "bass-leaps-to-fifth",
                                                                  "upper-voices-sustain"};

static bool table_is_hard(int which) {
    (void)which;
    return true;
}

static HarmonyRulePair g_pairs[HARMONY_ARPEGGIATING_RULE_COUNT];
static bool g_built[HARMONY_ARPEGGIATING_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"arpeggiating", NAMES, HARMONY_ARPEGGIATING_RULE_COUNT, table_is_hard,
                                       g_pairs, g_built, evaluate_candidate, score_candidate};

const Rule *harmony_arpeggiating_rule(HarmonyArpeggiatingRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_arpeggiating_constraint(HarmonyArpeggiatingRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_arpeggiating_preference(HarmonyArpeggiatingRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_arpeggiating_rule_name(HarmonyArpeggiatingRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool check_same_chord(const HarmonySixFour *s) {
    return s->chords[0].degree == s->chords[1].degree && s->chords[1].degree == s->chords[2].degree;
}

static bool check_bass_leaps_to_fifth(const HarmonySixFour *s) {
    int leap = s->steps[1][VOICE_PART_BASS] - s->steps[0][VOICE_PART_BASS];
    int fifth = (s->chords[1].degree - 1 + 4) % 7 + 1;
    return (leap > 1 || leap < -1) && harmony_step_degree(s->steps[1][VOICE_PART_BASS]) == fifth;
}

static bool check_upper_voices_sustain(const HarmonySixFour *s) {
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        if (s->steps[0][v] != s->steps[1][v] || s->steps[1][v] != s->steps[2][v]) {
            return false;
        }
    }
    return true;
}

bool harmony_arpeggiating_rule_holds(HarmonyArpeggiatingRule which, const HarmonySixFour *six_four) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_six_four_valid(six_four)) {
        return false;
    }
    switch (which) {
        case HARMONY_ARPEGGIATING_IS_SIX_FOUR:
            return harmony_six_four_middle_is_six_four(six_four);
        case HARMONY_ARPEGGIATING_SAME_CHORD:
            return check_same_chord(six_four);
        case HARMONY_ARPEGGIATING_BASS_LEAPS_TO_FIFTH:
            return check_bass_leaps_to_fifth(six_four);
        case HARMONY_ARPEGGIATING_UPPER_VOICES_SUSTAIN:
            return check_upper_voices_sustain(six_four);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_arpeggiating_rule_holds((HarmonyArpeggiatingRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonySixFour)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonySixFour)

bool harmony_arpeggiating_check(const HarmonySixFour *six_four, unsigned *hard_violations,
                                unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_six_four_valid(six_four)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, six_four, hard_violations, soft_violations);
    return true;
}
