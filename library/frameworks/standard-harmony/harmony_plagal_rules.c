#include "harmony_plagal_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_PLAGAL_RULE_COUNT] = {
    "progression", "chord-notes", "step-or-common-tone", "no-parallels", "soprano-do", "avoid-at-beginning"};

static bool table_is_hard(int which) {
    return which <= HARMONY_PLAGAL_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_PLAGAL_RULE_COUNT];
static bool g_built[HARMONY_PLAGAL_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"plagal", NAMES, HARMONY_PLAGAL_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate};

const Rule *harmony_plagal_rule(HarmonyPlagalRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_plagal_constraint(HarmonyPlagalRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_plagal_rule_name(HarmonyPlagalRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_plagal_motion_valid(const HarmonyPlagalMotion *m) {
    if (m == NULL || !harmony_mode_valid(m->mode) || !harmony_plagal_placement_valid(m->placement)) {
        return false;
    }
    return harmony_chord_valid(&m->chords[0]) && harmony_chord_valid(&m->chords[1]);
}

static int degree(const HarmonyPlagalMotion *m, int chord, int voice) {
    return harmony_step_degree(m->steps[chord][voice]);
}

static int count_degree(const HarmonyPlagalMotion *m, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(m, chord, v) == wanted;
    }
    return count;
}

/* The chord is the triad on `root`: only its notes, all three present, the bass on the root
   (or the third, when inverted), and the bass doubled in root position. */
static bool triad_notes(const HarmonyPlagalMotion *m, int chord, int root) {
    int third = (root - 1 + 2) % 7 + 1;
    int fifth = (root - 1 + 4) % 7 + 1;
    int present = count_degree(m, chord, root) + count_degree(m, chord, third) + count_degree(m, chord, fifth);
    if (present != VOICE_PART_COUNT || count_degree(m, chord, root) == 0 || count_degree(m, chord, third) == 0 ||
        count_degree(m, chord, fifth) == 0) {
        return false;
    }
    int bass = degree(m, chord, VOICE_PART_BASS);
    if (m->chords[chord].inversion == 0) {
        return bass == root && count_degree(m, chord, root) == 2;
    }
    return bass == third;
}

static bool check_chord_notes(const HarmonyPlagalMotion *m) {
    return triad_notes(m, 0, 4) && triad_notes(m, 1, 1);
}

static bool check_step_or_common_tone(const HarmonyPlagalMotion *m) {
    if (m->chords[1].inversion != 0) {
        return true;
    }
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        int move = m->steps[1][v] - m->steps[0][v];
        if (move < -1 || move > 1) {
            return false;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyPlagalMotion *m) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            if (harmony_steps_parallel(m->steps[0][a], m->steps[1][a], m->steps[0][b], m->steps[1][b])) {
                return false;
            }
        }
    }
    return true;
}

static bool check_soprano_do(const HarmonyPlagalMotion *m) {
    if (m->placement == HARMONY_PLAGAL_PHRASE_BEGINNING) {
        return true;
    }
    return degree(m, 0, VOICE_PART_SOPRANO) == 1 && degree(m, 1, VOICE_PART_SOPRANO) == 1;
}

bool harmony_plagal_rule_holds(HarmonyPlagalRule which, const HarmonyPlagalMotion *motion) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_plagal_motion_valid(motion)) {
        return false;
    }
    switch (which) {
        case HARMONY_PLAGAL_PROGRESSION:
            return harmony_plagal_progression_is(motion->chords, motion->placement);
        case HARMONY_PLAGAL_CHORD_NOTES:
            return check_chord_notes(motion);
        case HARMONY_PLAGAL_STEP_OR_COMMON_TONE:
            return check_step_or_common_tone(motion);
        case HARMONY_PLAGAL_NO_PARALLELS:
            return check_no_parallels(motion);
        case HARMONY_PLAGAL_SOPRANO_DO:
            return check_soprano_do(motion);
        case HARMONY_PLAGAL_AVOID_AT_BEGINNING:
            return motion->placement != HARMONY_PLAGAL_PHRASE_BEGINNING;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_plagal_rule_holds((HarmonyPlagalRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyPlagalMotion)

bool harmony_plagal_check(const HarmonyPlagalMotion *motion, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_plagal_motion_valid(motion)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, motion, hard_violations, soft_violations);
    return true;
}
