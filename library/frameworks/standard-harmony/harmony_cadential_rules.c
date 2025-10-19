#include "harmony_cadential_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"
#include "harmony_strong_predominants.h"

#define SOL 5
#define MI 3
#define DO 1
#define TI 7
#define FA 4

static const char *const NAMES[HARMONY_CADENTIAL_RULE_COUNT] = {
    "bass-stays",         "six-four-notes",    "resolves-six-five-four-three",
    "doubling-voice",     "minor-leading-tone", "no-parallels",
    "smooth-approach",    "no-leap-to-fourth", "approach-strong-predominant",
    "six-four-stronger-beat"};

static bool table_is_hard(int which) {
    return which <= HARMONY_CADENTIAL_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_CADENTIAL_RULE_COUNT];
static bool g_built[HARMONY_CADENTIAL_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"cadential", NAMES, HARMONY_CADENTIAL_RULE_COUNT, table_is_hard, g_pairs,
                                       g_built, evaluate_candidate, score_candidate};

const Rule *harmony_cadential_rule(HarmonyCadentialRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_cadential_constraint(HarmonyCadentialRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_cadential_preference(HarmonyCadentialRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_cadential_rule_name(HarmonyCadentialRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_cadential_valid(const HarmonyCadentialSixFour *cadential) {
    return cadential != NULL && harmony_mode_valid(cadential->mode) && beat_strength_valid(cadential->six_four_beat) &&
           beat_strength_valid(cadential->dominant_beat) &&
           (!cadential->has_approach || harmony_chord_valid(&cadential->approach));
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

static bool parallel_between(const int *from, const int *to) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            if (harmony_steps_parallel(from[a], to[a], from[b], to[b])) {
                return true;
            }
        }
    }
    return false;
}

static bool check_no_parallels(const HarmonyCadentialSixFour *c) {
    return !(c->has_approach && parallel_between(c->approach_steps, c->six_four_steps)) &&
           !parallel_between(c->six_four_steps, c->dominant_steps);
}

static int distance(int a, int b) {
    return a < b ? b - a : a - b;
}

/* No upper voice (the bass is sol, and leaps to it in the ordinary way) leaps into
   the 6/4 to a note of degree `only`, or to any note when `only` is 0. */
static bool no_leap_into(const HarmonyCadentialSixFour *c, int only) {
    if (!c->has_approach) {
        return true;
    }
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        bool member = only == 0 || degree(c->six_four_steps, v) == only;
        if (member && distance(c->approach_steps[v], c->six_four_steps[v]) > 1) {
            return false;
        }
    }
    return true;
}

static bool check_approach_strong_predominant(const HarmonyCadentialSixFour *c) {
    HarmonyStrongPredominant kind;
    return !c->has_approach ||
           (harmony_strong_predominant_of(c->mode, &c->approach, &kind) &&
            (kind == HARMONY_STRONG_PREDOMINANT_IV || kind == HARMONY_STRONG_PREDOMINANT_II6));
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
        case HARMONY_CADENTIAL_NO_PARALLELS:
            return check_no_parallels(cadential);
        case HARMONY_CADENTIAL_SMOOTH_APPROACH:
            return no_leap_into(cadential, 0);
        case HARMONY_CADENTIAL_NO_LEAP_TO_FOURTH:
            return no_leap_into(cadential, DO);
        case HARMONY_CADENTIAL_APPROACH_STRONG_PREDOMINANT:
            return check_approach_strong_predominant(cadential);
        case HARMONY_CADENTIAL_SIX_FOUR_STRONGER_BEAT:
            return beat_strength_is_stronger(cadential->six_four_beat, cadential->dominant_beat);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_cadential_rule_holds((HarmonyCadentialRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyCadentialSixFour)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyCadentialSixFour)

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
        if (table_is_hard(i) && !harmony_cadential_rule_holds((HarmonyCadentialRule)i, cadential)) {
            return false;
        }
    }
    *seventh = count_degree(cadential->dominant_steps, FA) > 0;
    return true;
}
