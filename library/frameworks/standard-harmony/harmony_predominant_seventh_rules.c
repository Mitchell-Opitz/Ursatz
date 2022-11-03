#include "harmony_predominant_seventh_rules.h"

#include <stddef.h>

#include "harmony_functions.h"
#include "harmony_rule_table.h"
#include "harmony_steps.h"

#define CHORD_COUNT 3

static const char *const NAMES[HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT] = {
    "progression", "chord-notes", "seventh-approach", "seventh-resolves",
    "no-parallels", "preceded-by-i6", "root-position", "vi7-connects"};

static bool table_is_hard(int which) {
    return which <= HARMONY_PREDOMINANT_SEVENTH_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT];
static bool g_built[HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"predominant-seventh", NAMES, HARMONY_PREDOMINANT_SEVENTH_RULE_COUNT,
                                       table_is_hard,         g_pairs, g_built};

const Rule *harmony_predominant_seventh_rule(HarmonyPredominantSeventhRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_predominant_seventh_constraint(HarmonyPredominantSeventhRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_predominant_seventh_rule_name(HarmonyPredominantSeventhRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_predominant_seventh_valid(const HarmonyPredominantSeventh *s) {
    if (s == NULL || !harmony_mode_valid(s->mode)) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!harmony_chord_valid(&s->chords[i])) {
            return false;
        }
    }
    return true;
}

static int degree(const HarmonyPredominantSeventh *s, int chord, int voice) {
    return harmony_step_degree(s->steps[chord][voice]);
}

static int count_degree(const HarmonyPredominantSeventh *s, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(s, chord, v) == wanted;
    }
    return count;
}

/* The degree `offset` steps above the root: third 2, fifth 4, seventh 6. */
static int above(int root, int offset) {
    return (root - 1 + offset) % 7 + 1;
}

static bool check_chord_notes(const HarmonyPredominantSeventh *s) {
    int root = s->chords[1].degree;
    int present = count_degree(s, 1, root) + count_degree(s, 1, above(root, 2)) + count_degree(s, 1, above(root, 4)) +
                  count_degree(s, 1, above(root, 6));
    if (present != VOICE_PART_COUNT || count_degree(s, 1, root) == 0 || count_degree(s, 1, above(root, 2)) == 0 ||
        count_degree(s, 1, above(root, 6)) == 0) {
        return false;
    }
    return degree(s, 1, VOICE_PART_BASS) == above(root, 2 * s->chords[1].inversion);
}

static bool check_seventh_approach(const HarmonyPredominantSeventh *s) {
    int seventh = above(s->chords[1].degree, 6);
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int move = s->steps[1][v] - s->steps[0][v];
        if (degree(s, 1, v) == seventh && (move < -1 || move > 1)) {
            return false;
        }
    }
    return true;
}

static bool check_seventh_resolves(const HarmonyPredominantSeventh *s) {
    int seventh = above(s->chords[1].degree, 6);
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (degree(s, 1, v) == seventh && s->steps[2][v] - s->steps[1][v] != -1) {
            return false;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyPredominantSeventh *s) {
    for (int c = 0; c + 1 < CHORD_COUNT; c++) {
        for (int a = 0; a < VOICE_PART_COUNT; a++) {
            for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
                if (harmony_steps_parallel(s->steps[c][a], s->steps[c + 1][a], s->steps[c][b], s->steps[c + 1][b])) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool check_preceded_by_i6(const HarmonyPredominantSeventh *s) {
    if (s->chords[1].degree != 2) {
        return true;
    }
    const HarmonyChord *before = &s->chords[0];
    return !(before->degree == 1 && before->inversion == 0 && !before->seventh);
}

static bool check_root_position(const HarmonyPredominantSeventh *s, HarmonyPredominantSeventhKind kind) {
    if (kind != HARMONY_PREDOMINANT_SEVENTH_IV7 && kind != HARMONY_PREDOMINANT_SEVENTH_VI7) {
        return true;
    }
    return s->chords[1].inversion == 0;
}

static bool check_vi7_connects(const HarmonyPredominantSeventh *s, HarmonyPredominantSeventhKind kind) {
    if (kind != HARMONY_PREDOMINANT_SEVENTH_VI7) {
        return true;
    }
    HarmonyPredominantStrength strength;
    return s->chords[0].degree != 5 && harmony_predominant_strength(s->mode, &s->chords[2], &strength) &&
           strength == HARMONY_PREDOMINANT_STRONG;
}

bool harmony_predominant_seventh_rule_holds(HarmonyPredominantSeventhRule which,
                                            const HarmonyPredominantSeventh *subject) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_predominant_seventh_valid(subject)) {
        return false;
    }
    HarmonyPredominantSeventhKind kind;
    bool classified = harmony_predominant_seventh_of(&subject->chords[1], &kind);
    switch (which) {
        case HARMONY_PREDOMINANT_SEVENTH_PROGRESSION:
            return classified;
        case HARMONY_PREDOMINANT_SEVENTH_CHORD_NOTES:
            return classified && check_chord_notes(subject);
        case HARMONY_PREDOMINANT_SEVENTH_SEVENTH_APPROACH:
            return classified && check_seventh_approach(subject);
        case HARMONY_PREDOMINANT_SEVENTH_SEVENTH_RESOLVES:
            return classified && check_seventh_resolves(subject);
        case HARMONY_PREDOMINANT_SEVENTH_NO_PARALLELS:
            return check_no_parallels(subject);
        case HARMONY_PREDOMINANT_SEVENTH_PRECEDED_BY_I6:
            return check_preceded_by_i6(subject);
        case HARMONY_PREDOMINANT_SEVENTH_ROOT_POSITION:
            return !classified || check_root_position(subject, kind);
        case HARMONY_PREDOMINANT_SEVENTH_VI7_CONNECTS:
            return !classified || check_vi7_connects(subject, kind);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_predominant_seventh_rule_holds((HarmonyPredominantSeventhRule)which, subject);
}

bool harmony_predominant_seventh_check(const HarmonyPredominantSeventh *subject, unsigned *hard_violations,
                                       unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_predominant_seventh_valid(subject)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, subject, hard_violations, soft_violations);
    return true;
}
