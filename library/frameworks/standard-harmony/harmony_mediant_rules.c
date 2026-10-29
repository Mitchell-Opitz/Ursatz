#include "harmony_mediant_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

#define CHORD_COUNT 3

static const char *const NAMES[HARMONY_MEDIANT_RULE_COUNT] = {
    "progression",   "bass-line",     "chord-notes",         "descending-leading-tone",
    "no-parallels",  "root-position", "through-predominant", "avoid-v-in-minor"};

static bool table_is_hard(int which) {
    return which <= HARMONY_MEDIANT_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_MEDIANT_RULE_COUNT];
static UrsatzOnce g_built[HARMONY_MEDIANT_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"mediant", NAMES, HARMONY_MEDIANT_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate, score_candidate};

const Rule *harmony_mediant_rule(HarmonyMediantRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_mediant_constraint(HarmonyMediantRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_mediant_preference(HarmonyMediantRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_mediant_rule_name(HarmonyMediantRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_mediant_valid(const HarmonyMediant *m) {
    if (m == NULL || !harmony_mode_valid(m->mode)) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!harmony_chord_valid(&m->chords[i])) {
            return false;
        }
    }
    return true;
}

static int degree(const HarmonyMediant *m, int chord, int voice) {
    return harmony_step_degree(m->steps[chord][voice]);
}

static int count_degree(const HarmonyMediant *m, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(m, chord, v) == wanted;
    }
    return count;
}

static int third_of(int root) {
    return (root - 1 + 2) % 7 + 1;
}

static bool check_bass_line(const HarmonyMediant *m) {
    for (int i = 0; i < CHORD_COUNT; i++) {
        int root = m->chords[i].degree;
        int bass = m->chords[i].inversion == 0 ? root : third_of(root);
        if (degree(m, i, VOICE_PART_BASS) != bass) {
            return false;
        }
    }
    return true;
}

static bool check_chord_notes(const HarmonyMediant *m) {
    for (int i = 0; i < CHORD_COUNT; i++) {
        int root = m->chords[i].degree;
        int third = third_of(root);
        int fifth = (root - 1 + 4) % 7 + 1;
        int present = count_degree(m, i, root) + count_degree(m, i, third) + count_degree(m, i, fifth);
        if (present != VOICE_PART_COUNT || count_degree(m, i, root) == 0 || count_degree(m, i, third) == 0) {
            return false;
        }
    }
    return true;
}

/* An upper voice is on ti over iii; each is do over the I and goes a step down to la over the
   predominant, or stays on ti into V. */
static bool check_descending_leading_tone(const HarmonyMediant *m, HarmonyMediantKind kind) {
    int found = 0;
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        if (degree(m, 1, v) != 7) {
            continue;
        }
        found++;
        if (degree(m, 0, v) != 1 || m->steps[0][v] - m->steps[1][v] != 1) {
            return false;
        }
        int move = m->steps[2][v] - m->steps[1][v];
        if (move != (kind == HARMONY_MEDIANT_TO_PREDOMINANT ? -1 : 0)) {
            return false;
        }
    }
    return found > 0;
}

static bool check_no_parallels(const HarmonyMediant *m) {
    for (int c = 0; c + 1 < CHORD_COUNT; c++) {
        for (int a = 0; a < VOICE_PART_COUNT; a++) {
            for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
                if (harmony_steps_parallel(m->steps[c][a], m->steps[c + 1][a], m->steps[c][b], m->steps[c + 1][b])) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool harmony_mediant_rule_holds(HarmonyMediantRule which, const HarmonyMediant *mediant) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_mediant_valid(mediant)) {
        return false;
    }
    HarmonyMediantKind kind;
    bool classified = harmony_mediant_of(mediant->mode, mediant->chords, &kind);
    switch (which) {
        case HARMONY_MEDIANT_PROGRESSION:
            return classified;
        case HARMONY_MEDIANT_BASS_LINE:
            return classified && check_bass_line(mediant);
        case HARMONY_MEDIANT_CHORD_NOTES:
            return classified && check_chord_notes(mediant);
        case HARMONY_MEDIANT_DESCENDING_LEADING_TONE:
            return classified && check_descending_leading_tone(mediant, kind);
        case HARMONY_MEDIANT_NO_PARALLELS:
            return check_no_parallels(mediant);
        case HARMONY_MEDIANT_ROOT_POSITION:
            return mediant->chords[1].inversion == 0;
        case HARMONY_MEDIANT_THROUGH_PREDOMINANT:
            return classified && kind == HARMONY_MEDIANT_TO_PREDOMINANT;
        case HARMONY_MEDIANT_AVOID_V_IN_MINOR:
            return !(classified && kind == HARMONY_MEDIANT_TO_DOMINANT && mediant->mode == HARMONY_MODE_MINOR);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_mediant_rule_holds((HarmonyMediantRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyMediant)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyMediant)

bool harmony_mediant_check(const HarmonyMediant *mediant, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_mediant_valid(mediant)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, mediant, hard_violations, soft_violations);
    return true;
}
