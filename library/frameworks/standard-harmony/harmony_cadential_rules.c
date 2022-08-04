#include "harmony_cadential_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"

#define SOL 5
#define MI 3
#define DO 1
#define TI 7
#define FA 4

static const char *const NAMES[HARMONY_CADENTIAL_RULE_COUNT] = {
    "bass-stays", "six-four-notes", "resolves-six-five-four-three", "doubling-voice", "minor-leading-tone"};

static bool table_is_hard(int which) {
    (void)which;
    return true;
}

static HarmonyRulePair g_pairs[HARMONY_CADENTIAL_RULE_COUNT];
static bool g_built[HARMONY_CADENTIAL_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"cadential", NAMES, HARMONY_CADENTIAL_RULE_COUNT, table_is_hard, g_pairs,
                                       g_built};

const Rule *harmony_cadential_rule(HarmonyCadentialRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_cadential_constraint(HarmonyCadentialRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_cadential_rule_name(HarmonyCadentialRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_cadential_valid(const HarmonyCadentialSixFour *cadential) {
    return cadential != NULL && harmony_mode_valid(cadential->mode);
}

static int degree(const int *steps, int voice) {
    return harmony_step_degree(steps[voice]);
}

static int count_degree(const int *steps, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(steps, v) == wanted;
    }
    return count;
}

static bool check_bass_stays(const HarmonyCadentialSixFour *c) {
    return degree(c->six_four_steps, VOICE_PART_BASS) == SOL &&
           c->dominant_steps[VOICE_PART_BASS] == c->six_four_steps[VOICE_PART_BASS];
}

static bool check_six_four_notes(const HarmonyCadentialSixFour *c) {
    return count_degree(c->six_four_steps, SOL) == 2 && count_degree(c->six_four_steps, MI) == 1 &&
           count_degree(c->six_four_steps, DO) == 1;
}

/* The sixth above the bass (mi) falls a step, to the fifth (re); the fourth above
   (do) falls a step, to the third (ti). */
static bool check_resolves(const HarmonyCadentialSixFour *c) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int d = degree(c->six_four_steps, v);
        if ((d == MI || d == DO) && c->dominant_steps[v] != c->six_four_steps[v] - 1) {
            return false;
        }
    }
    return true;
}

/* The voice that doubles the bass (an upper voice on sol) stays, or falls a step to
   the seventh. */
static bool check_doubling_voice(const HarmonyCadentialSixFour *c) {
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        if (degree(c->six_four_steps, v) == SOL && c->dominant_steps[v] != c->six_four_steps[v] &&
            c->dominant_steps[v] != c->six_four_steps[v] - 1) {
            return false;
        }
    }
    return true;
}

bool harmony_cadential_rule_holds(HarmonyCadentialRule which, const HarmonyCadentialSixFour *cadential) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_cadential_valid(cadential)) {
        return false;
    }
    switch (which) {
        case HARMONY_CADENTIAL_BASS_STAYS:
            return check_bass_stays(cadential);
        case HARMONY_CADENTIAL_SIX_FOUR_NOTES:
            return check_six_four_notes(cadential);
        case HARMONY_CADENTIAL_RESOLVES_SIX_FIVE_FOUR_THREE:
            return check_resolves(cadential);
        case HARMONY_CADENTIAL_DOUBLING_VOICE:
            return check_doubling_voice(cadential);
        case HARMONY_CADENTIAL_MINOR_LEADING_TONE:
            return cadential->mode == HARMONY_MODE_MAJOR || cadential->dominant_has_leading_tone;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_cadential_rule_holds((HarmonyCadentialRule)which, subject);
}

bool harmony_cadential_check(const HarmonyCadentialSixFour *cadential, unsigned *hard_violations,
                             unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_cadential_valid(cadential)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, cadential, hard_violations, soft_violations);
    return true;
}

bool harmony_cadential_resolves_to_seventh(const HarmonyCadentialSixFour *cadential, bool *seventh) {
    if (seventh == NULL || !harmony_cadential_valid(cadential)) {
        return false;
    }
    for (int i = 0; i < HARMONY_CADENTIAL_RULE_COUNT; i++) {
        if (!harmony_cadential_rule_holds((HarmonyCadentialRule)i, cadential)) {
            return false;
        }
    }
    *seventh = count_degree(cadential->dominant_steps, FA) > 0;
    return true;
}
