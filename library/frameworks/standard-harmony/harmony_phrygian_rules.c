#include "harmony_phrygian_rules.h"

#include <stddef.h>

#include "harmony_phrygian.h"
#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_PHRYGIAN_RULE_COUNT] = {"progression", "chord-notes", "doubled-do",
                                                               "no-augmented-second", "no-parallels"};

static HarmonyRulePair g_pairs[HARMONY_PHRYGIAN_RULE_COUNT];
static UrsatzOnce g_built[HARMONY_PHRYGIAN_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"phrygian", NAMES, HARMONY_PHRYGIAN_RULE_COUNT, NULL, g_pairs, g_built,
                                       evaluate_candidate, score_candidate};

const Rule *harmony_phrygian_rule(HarmonyPhrygianRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_phrygian_constraint(HarmonyPhrygianRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_phrygian_preference(HarmonyPhrygianRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_phrygian_rule_name(HarmonyPhrygianRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_phrygian_valid(const HarmonyPhrygian *p) {
    return p != NULL && harmony_mode_valid(p->mode) && harmony_chord_valid(&p->chords[0]) &&
           harmony_chord_valid(&p->chords[1]);
}

static int degree(const HarmonyPhrygian *p, int chord, int voice) {
    return harmony_step_degree(p->steps[chord][voice]);
}

static int count_degree(const HarmonyPhrygian *p, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(p, chord, v) == wanted;
    }
    return count;
}

/* Only the chord's notes (the seventh too, in V7), its root and third present, the bass on
   `bass`. */
static bool chord_notes(const HarmonyPhrygian *p, int chord, int root, int bass) {
    int third = (root - 1 + 2) % 7 + 1;
    int fifth = (root - 1 + 4) % 7 + 1;
    int seventh = (root - 1 + 6) % 7 + 1;
    int present = count_degree(p, chord, root) + count_degree(p, chord, third) + count_degree(p, chord, fifth);
    if (p->chords[chord].seventh) {
        present += count_degree(p, chord, seventh);
    }
    return present == VOICE_PART_COUNT && count_degree(p, chord, root) > 0 && count_degree(p, chord, third) > 0 &&
           degree(p, chord, VOICE_PART_BASS) == bass;
}

static bool check_chord_notes(const HarmonyPhrygian *p) {
    return chord_notes(p, 0, 4, 6) && chord_notes(p, 1, 5, 5);
}

static bool check_no_augmented_second(const HarmonyPhrygian *p) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (degree(p, 0, v) == 6 && degree(p, 1, v) == 7 && p->steps[1][v] - p->steps[0][v] == 1) {
            return false;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyPhrygian *p) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            if (harmony_steps_parallel(p->steps[0][a], p->steps[1][a], p->steps[0][b], p->steps[1][b])) {
                return false;
            }
        }
    }
    return true;
}

bool harmony_phrygian_rule_holds(HarmonyPhrygianRule which, const HarmonyPhrygian *phrygian) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_phrygian_valid(phrygian)) {
        return false;
    }
    switch (which) {
        case HARMONY_PHRYGIAN_PROGRESSION:
            return harmony_phrygian_half_cadence_is(phrygian->mode, phrygian->chords);
        case HARMONY_PHRYGIAN_CHORD_NOTES:
            return check_chord_notes(phrygian);
        case HARMONY_PHRYGIAN_DOUBLED_DO:
            return count_degree(phrygian, 0, 1) == 2;
        case HARMONY_PHRYGIAN_NO_AUGMENTED_SECOND:
            return check_no_augmented_second(phrygian);
        case HARMONY_PHRYGIAN_NO_PARALLELS:
            return check_no_parallels(phrygian);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_phrygian_rule_holds((HarmonyPhrygianRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyPhrygian)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyPhrygian)

bool harmony_phrygian_check(const HarmonyPhrygian *phrygian, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_phrygian_valid(phrygian)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, phrygian, hard_violations, soft_violations);
    return true;
}
