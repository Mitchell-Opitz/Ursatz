#include "harmony_passing_rules.h"

#include "harmony_functions.h"
#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_PASSING_RULE_COUNT] = {
    "is-six-four",   "stepwise-bass",   "same-function",           "six-four-notes",
    "prolongs-tonic-or-predominant", "soprano-by-step", "inner-voices-move-little"};

static bool table_is_hard(int which) {
    return which <= HARMONY_PASSING_SIX_FOUR_NOTES;
}

static HarmonyRulePair g_pairs[HARMONY_PASSING_RULE_COUNT];
static bool g_built[HARMONY_PASSING_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"passing", NAMES, HARMONY_PASSING_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *harmony_passing_rule(HarmonyPassingRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_passing_constraint(HarmonyPassingRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_passing_rule_name(HarmonyPassingRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static int magnitude(int n) {
    return n < 0 ? -n : n;
}

static bool check_stepwise_bass(const HarmonySixFour *s) {
    int first = s->steps[1][VOICE_PART_BASS] - s->steps[0][VOICE_PART_BASS];
    int second = s->steps[2][VOICE_PART_BASS] - s->steps[1][VOICE_PART_BASS];
    return magnitude(first) == 1 && first == second;
}

static bool functions(const HarmonySixFour *s, HarmonicFunction *before, HarmonicFunction *after) {
    return harmony_function_of(s->mode, &s->chords[0], before) && harmony_function_of(s->mode, &s->chords[2], after);
}

static int count_degree(const HarmonySixFour *s, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += harmony_step_degree(s->steps[chord][v]) == wanted;
    }
    return count;
}

/* The bass doubled once, and the fourth and sixth above it once each. */
static bool check_six_four_notes(const HarmonySixFour *s) {
    int bass = harmony_step_degree(s->steps[1][VOICE_PART_BASS]);
    int fourth = (bass - 1 + 3) % 7 + 1;
    int sixth = (bass - 1 + 5) % 7 + 1;
    return count_degree(s, 1, bass) == 2 && count_degree(s, 1, fourth) == 1 && count_degree(s, 1, sixth) == 1;
}

static bool check_soprano_by_step(const HarmonySixFour *s) {
    return magnitude(s->steps[1][VOICE_PART_SOPRANO] - s->steps[0][VOICE_PART_SOPRANO]) == 1 &&
           magnitude(s->steps[2][VOICE_PART_SOPRANO] - s->steps[1][VOICE_PART_SOPRANO]) == 1;
}

static bool check_inner_voices(const HarmonySixFour *s) {
    for (int v = VOICE_PART_ALTO; v <= VOICE_PART_TENOR; v++) {
        if (magnitude(s->steps[1][v] - s->steps[0][v]) > 1 || magnitude(s->steps[2][v] - s->steps[1][v]) > 1) {
            return false;
        }
    }
    return true;
}

bool harmony_passing_rule_holds(HarmonyPassingRule which, const HarmonySixFour *six_four) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_six_four_valid(six_four)) {
        return false;
    }
    HarmonicFunction before, after;
    switch (which) {
        case HARMONY_PASSING_IS_SIX_FOUR:
            return harmony_six_four_middle_is_six_four(six_four);
        case HARMONY_PASSING_STEPWISE_BASS:
            return check_stepwise_bass(six_four);
        case HARMONY_PASSING_SAME_FUNCTION:
            return functions(six_four, &before, &after) && before == after;
        case HARMONY_PASSING_SIX_FOUR_NOTES:
            return check_six_four_notes(six_four);
        case HARMONY_PASSING_PROLONGS_TONIC_OR_PREDOMINANT:
            return functions(six_four, &before, &after) && before == after &&
                   (before == HARMONIC_FUNCTION_TONIC || before == HARMONIC_FUNCTION_PREDOMINANT);
        case HARMONY_PASSING_SOPRANO_BY_STEP:
            return check_soprano_by_step(six_four);
        case HARMONY_PASSING_INNER_VOICES_MOVE_LITTLE:
            return check_inner_voices(six_four);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_passing_rule_holds((HarmonyPassingRule)which, subject);
}

bool harmony_passing_check(const HarmonySixFour *six_four, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_six_four_valid(six_four)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, six_four, hard_violations, soft_violations);
    return true;
}
