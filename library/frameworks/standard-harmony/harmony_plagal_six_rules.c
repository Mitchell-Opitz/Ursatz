#include "harmony_plagal_six_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

#define CHORD_COUNT 3

static const char *const NAMES[HARMONY_PLAGAL_SIX_RULE_COUNT] = {
    "progression", "bass-line", "chord-notes", "step-or-common-tone", "no-parallels", "soprano-mi-fa-sol"};

static bool table_is_hard(int which) {
    return which <= HARMONY_PLAGAL_SIX_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_PLAGAL_SIX_RULE_COUNT];
static bool g_built[HARMONY_PLAGAL_SIX_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"plagal-six", NAMES, HARMONY_PLAGAL_SIX_RULE_COUNT, table_is_hard, g_pairs,
                                       g_built, evaluate_candidate};

const Rule *harmony_plagal_six_rule(HarmonyPlagalSixRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_plagal_six_constraint(HarmonyPlagalSixRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_plagal_six_rule_name(HarmonyPlagalSixRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool is_triad(const HarmonyChord *chord, int degree, int inversion) {
    return chord->degree == degree && chord->inversion == inversion && !chord->seventh && !chord->cadential;
}

bool harmony_plagal_six_progression_is(const HarmonyChord chords[3]) {
    if (chords == NULL) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!harmony_chord_valid(&chords[i])) {
            return false;
        }
    }
    return is_triad(&chords[0], 1, 0) && is_triad(&chords[1], 4, 1) && is_triad(&chords[2], 1, 1);
}

bool harmony_plagal_six_valid(const HarmonyPlagalSix *p) {
    if (p == NULL || !harmony_mode_valid(p->mode)) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!harmony_chord_valid(&p->chords[i])) {
            return false;
        }
    }
    return true;
}

static int degree(const HarmonyPlagalSix *p, int chord, int voice) {
    return harmony_step_degree(p->steps[chord][voice]);
}

static int count_degree(const HarmonyPlagalSix *p, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(p, chord, v) == wanted;
    }
    return count;
}

/* A triad: only its notes, its root and third present, and the bass on `bass`. */
static bool triad_notes(const HarmonyPlagalSix *p, int chord, int root, int bass) {
    int third = (root - 1 + 2) % 7 + 1;
    int fifth = (root - 1 + 4) % 7 + 1;
    int present = count_degree(p, chord, root) + count_degree(p, chord, third) + count_degree(p, chord, fifth);
    return present == VOICE_PART_COUNT && count_degree(p, chord, root) > 0 && count_degree(p, chord, third) > 0 &&
           degree(p, chord, VOICE_PART_BASS) == bass;
}

static bool check_bass_line(const HarmonyPlagalSix *p) {
    static const int line[CHORD_COUNT] = {1, 6, 3};
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (degree(p, i, VOICE_PART_BASS) != line[i]) {
            return false;
        }
    }
    for (int i = 0; i + 1 < CHORD_COUNT; i++) {
        if (p->steps[i + 1][VOICE_PART_BASS] - p->steps[i][VOICE_PART_BASS] > -2) {
            return false;
        }
    }
    return true;
}

static bool check_chord_notes(const HarmonyPlagalSix *p) {
    return triad_notes(p, 0, 1, 1) && triad_notes(p, 1, 4, 6) && triad_notes(p, 2, 1, 3);
}

static bool check_step_or_common_tone(const HarmonyPlagalSix *p) {
    for (int c = 0; c + 1 < CHORD_COUNT; c++) {
        for (int v = 0; v < VOICE_PART_BASS; v++) {
            int move = p->steps[c + 1][v] - p->steps[c][v];
            if (move < -1 || move > 1) {
                return false;
            }
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyPlagalSix *p) {
    for (int c = 0; c + 1 < CHORD_COUNT; c++) {
        for (int a = 0; a < VOICE_PART_COUNT; a++) {
            for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
                if (harmony_steps_parallel(p->steps[c][a], p->steps[c + 1][a], p->steps[c][b], p->steps[c + 1][b])) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool check_soprano(const HarmonyPlagalSix *p) {
    static const int line[CHORD_COUNT] = {3, 4, 5};
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (degree(p, i, VOICE_PART_SOPRANO) != line[i]) {
            return false;
        }
    }
    return true;
}

bool harmony_plagal_six_rule_holds(HarmonyPlagalSixRule which, const HarmonyPlagalSix *plagal) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_plagal_six_valid(plagal)) {
        return false;
    }
    switch (which) {
        case HARMONY_PLAGAL_SIX_PROGRESSION:
            return harmony_plagal_six_progression_is(plagal->chords);
        case HARMONY_PLAGAL_SIX_BASS_LINE:
            return check_bass_line(plagal);
        case HARMONY_PLAGAL_SIX_CHORD_NOTES:
            return check_chord_notes(plagal);
        case HARMONY_PLAGAL_SIX_STEP_OR_COMMON_TONE:
            return check_step_or_common_tone(plagal);
        case HARMONY_PLAGAL_SIX_NO_PARALLELS:
            return check_no_parallels(plagal);
        case HARMONY_PLAGAL_SIX_SOPRANO_MI_FA_SOL:
            return check_soprano(plagal);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_plagal_six_rule_holds((HarmonyPlagalSixRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyPlagalSix)

bool harmony_plagal_six_check(const HarmonyPlagalSix *plagal, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_plagal_six_valid(plagal)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, plagal, hard_violations, soft_violations);
    return true;
}
