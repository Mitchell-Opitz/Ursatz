#include "harmony_iv6_prolongation_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

#define CHORD_COUNT 4

static const char *const NAMES[HARMONY_IV6_PROLONGATION_RULE_COUNT] = {
    "progression",  "bass-line",    "chord-notes", "no-parallels",
    "no-doubled-leading-tone", "soprano-line", "prefer-v6",  "avoid-in-minor"};

static bool table_is_hard(int which) {
    return which <= HARMONY_IV6_PROLONGATION_NO_DOUBLED_LEADING_TONE;
}

static HarmonyRulePair g_pairs[HARMONY_IV6_PROLONGATION_RULE_COUNT];
static UrsatzOnce g_built[HARMONY_IV6_PROLONGATION_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"iv6-prolongation", NAMES, HARMONY_IV6_PROLONGATION_RULE_COUNT, table_is_hard,
                                       g_pairs, g_built, evaluate_candidate, score_candidate};

const Rule *harmony_iv6_prolongation_rule(HarmonyIv6ProlongationRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_iv6_prolongation_constraint(HarmonyIv6ProlongationRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_iv6_prolongation_preference(HarmonyIv6ProlongationRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_iv6_prolongation_rule_name(HarmonyIv6ProlongationRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_iv6_prolongation_valid(const HarmonyIv6Prolongation *p) {
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

static int degree(const HarmonyIv6Prolongation *p, int chord, int voice) {
    return harmony_step_degree(p->steps[chord][voice]);
}

static int count_degree(const HarmonyIv6Prolongation *p, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(p, chord, v) == wanted;
    }
    return count;
}

/* A triad: only its notes, its root and third present, and the bass on `bass`. */
static bool triad_notes(const HarmonyIv6Prolongation *p, int chord, int root, int bass) {
    int third = (root - 1 + 2) % 7 + 1;
    int fifth = (root - 1 + 4) % 7 + 1;
    int present = count_degree(p, chord, root) + count_degree(p, chord, third) + count_degree(p, chord, fifth);
    return present == VOICE_PART_COUNT && count_degree(p, chord, root) > 0 && count_degree(p, chord, third) > 0 &&
           degree(p, chord, VOICE_PART_BASS) == bass;
}

/* The bass scale degree of each chord: do la sol do, or do la ti do. */
static bool bass_degrees(const HarmonyIv6Prolongation *p, int out[CHORD_COUNT]) {
    HarmonyIv6ProlongationKind kind;
    if (!harmony_iv6_prolongation_of(p->chords, &kind)) {
        return false;
    }
    out[0] = 1;
    out[1] = 6;
    out[2] = kind == HARMONY_IV6_PROLONGATION_V ? 5 : 7;
    out[3] = 1;
    return true;
}

static bool check_bass_line(const HarmonyIv6Prolongation *p) {
    int want[CHORD_COUNT];
    if (!bass_degrees(p, want)) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (degree(p, i, VOICE_PART_BASS) != want[i]) {
            return false;
        }
    }
    return true;
}

static bool check_chord_notes(const HarmonyIv6Prolongation *p) {
    int bass[CHORD_COUNT];
    if (!bass_degrees(p, bass)) {
        return false;
    }
    int root[CHORD_COUNT] = {1, 4, 5, 1};
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!triad_notes(p, i, root[i], bass[i])) {
            return false;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyIv6Prolongation *p) {
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

static bool check_soprano_line(const HarmonyIv6Prolongation *p) {
    static const int line[CHORD_COUNT] = {1, 1, 2, 1};
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (degree(p, i, VOICE_PART_SOPRANO) != line[i]) {
            return false;
        }
    }
    return true;
}

bool harmony_iv6_prolongation_rule_holds(HarmonyIv6ProlongationRule which, const HarmonyIv6Prolongation *prolongation) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_iv6_prolongation_valid(prolongation)) {
        return false;
    }
    HarmonyIv6ProlongationKind kind;
    switch (which) {
        case HARMONY_IV6_PROLONGATION_PROGRESSION:
            return harmony_iv6_prolongation_of(prolongation->chords, &kind);
        case HARMONY_IV6_PROLONGATION_BASS_LINE:
            return check_bass_line(prolongation);
        case HARMONY_IV6_PROLONGATION_CHORD_NOTES:
            return check_chord_notes(prolongation);
        case HARMONY_IV6_PROLONGATION_NO_PARALLELS:
            return check_no_parallels(prolongation);
        case HARMONY_IV6_PROLONGATION_NO_DOUBLED_LEADING_TONE:
            return count_degree(prolongation, 2, 7) <= 1;
        case HARMONY_IV6_PROLONGATION_SOPRANO_LINE:
            return check_soprano_line(prolongation);
        case HARMONY_IV6_PROLONGATION_PREFER_V6:
            return prolongation->chords[2].inversion == 1;
        case HARMONY_IV6_PROLONGATION_AVOID_IN_MINOR:
            return prolongation->mode == HARMONY_MODE_MAJOR;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_iv6_prolongation_rule_holds((HarmonyIv6ProlongationRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyIv6Prolongation)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyIv6Prolongation)

bool harmony_iv6_prolongation_check(const HarmonyIv6Prolongation *prolongation, unsigned *hard_violations,
                                    unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_iv6_prolongation_valid(prolongation)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, prolongation, hard_violations, soft_violations);
    return true;
}
