#include "harmony_deceptive_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_DECEPTIVE_RULE_COUNT] = {"progression",     "chord-notes",  "resolves",
                                                                "contrary-motion", "no-parallels", "prefer-vi"};

static bool table_is_hard(int which) {
    return which <= HARMONY_DECEPTIVE_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_DECEPTIVE_RULE_COUNT];
static bool g_built[HARMONY_DECEPTIVE_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"deceptive", NAMES, HARMONY_DECEPTIVE_RULE_COUNT, table_is_hard, g_pairs,
                                       g_built, evaluate_candidate};

const Rule *harmony_deceptive_rule(HarmonyDeceptiveRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_deceptive_constraint(HarmonyDeceptiveRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_deceptive_rule_name(HarmonyDeceptiveRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_deceptive_motion_valid(const HarmonyDeceptiveMotion *m) {
    return m != NULL && harmony_mode_valid(m->mode) && harmony_chord_valid(&m->chords[0]) &&
           harmony_chord_valid(&m->chords[1]);
}

static int degree(const HarmonyDeceptiveMotion *m, int chord, int voice) {
    return harmony_step_degree(m->steps[chord][voice]);
}

static int count_degree(const HarmonyDeceptiveMotion *m, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(m, chord, v) == wanted;
    }
    return count;
}

/* A triad: only its notes, its root and third present, and the bass on `bass`. */
static bool triad_notes(const HarmonyDeceptiveMotion *m, int chord, int root, int bass) {
    int third = (root - 1 + 2) % 7 + 1;
    int fifth = (root - 1 + 4) % 7 + 1;
    int present = count_degree(m, chord, root) + count_degree(m, chord, third) + count_degree(m, chord, fifth);
    return present == VOICE_PART_COUNT && count_degree(m, chord, root) > 0 && count_degree(m, chord, third) > 0 &&
           degree(m, chord, VOICE_PART_BASS) == bass;
}

/* V7: sol, ti, re, and fa once each, with sol in the bass. */
static bool dominant_seventh_notes(const HarmonyDeceptiveMotion *m) {
    return count_degree(m, 0, 5) == 1 && count_degree(m, 0, 7) == 1 && count_degree(m, 0, 2) == 1 &&
           count_degree(m, 0, 4) == 1 && degree(m, 0, VOICE_PART_BASS) == 5;
}

static bool check_chord_notes(const HarmonyDeceptiveMotion *m) {
    bool first = m->chords[0].seventh ? dominant_seventh_notes(m) : triad_notes(m, 0, 5, 5);
    bool second = m->chords[1].degree == 6 ? triad_notes(m, 1, 6, 6) : triad_notes(m, 1, 4, 6);
    return first && second;
}

static int move(const HarmonyDeceptiveMotion *m, int voice) {
    return m->steps[1][voice] - m->steps[0][voice];
}

static bool check_resolves(const HarmonyDeceptiveMotion *m) {
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        int d = degree(m, 0, v);
        if (d == 7 && move(m, v) != 1) {
            return false;
        }
        if (d == 4 && m->chords[0].seventh && m->chords[1].degree == 6 && move(m, v) != -1) {
            return false;
        }
    }
    return true;
}

static bool check_contrary_motion(const HarmonyDeceptiveMotion *m) {
    if (m->chords[1].degree != 6) {
        return true;
    }
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        if (degree(m, 0, v) != 7 && move(m, v) >= 0) {
            return false;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyDeceptiveMotion *m) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            if (harmony_steps_parallel(m->steps[0][a], m->steps[1][a], m->steps[0][b], m->steps[1][b])) {
                return false;
            }
        }
    }
    return true;
}

bool harmony_deceptive_rule_holds(HarmonyDeceptiveRule which, const HarmonyDeceptiveMotion *motion) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_deceptive_motion_valid(motion)) {
        return false;
    }
    HarmonyDeceptiveKind kind;
    switch (which) {
        case HARMONY_DECEPTIVE_PROGRESSION:
            return harmony_deceptive_of(motion->chords, &kind);
        case HARMONY_DECEPTIVE_CHORD_NOTES:
            return harmony_deceptive_of(motion->chords, &kind) && check_chord_notes(motion);
        case HARMONY_DECEPTIVE_RESOLVES:
            return check_resolves(motion);
        case HARMONY_DECEPTIVE_CONTRARY_MOTION:
            return check_contrary_motion(motion);
        case HARMONY_DECEPTIVE_NO_PARALLELS:
            return check_no_parallels(motion);
        case HARMONY_DECEPTIVE_PREFER_VI:
            return motion->chords[1].degree == 6;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_deceptive_rule_holds((HarmonyDeceptiveRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyDeceptiveMotion)

bool harmony_deceptive_check(const HarmonyDeceptiveMotion *motion, unsigned *hard_violations,
                             unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_deceptive_motion_valid(motion)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, motion, hard_violations, soft_violations);
    return true;
}
