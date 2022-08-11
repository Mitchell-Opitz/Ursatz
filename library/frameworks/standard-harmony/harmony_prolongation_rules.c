#include "harmony_prolongation_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_PROLONGATION_RULE_COUNT] = {"progression", "bass-line", "chord-notes"};

static bool table_is_hard(int which) {
    (void)which;
    return true;
}

static HarmonyRulePair g_pairs[HARMONY_PROLONGATION_RULE_COUNT];
static bool g_built[HARMONY_PROLONGATION_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"prolongation", NAMES, HARMONY_PROLONGATION_RULE_COUNT, table_is_hard,
                                       g_pairs, g_built};

const Rule *harmony_prolongation_rule(HarmonyProlongationRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_prolongation_constraint(HarmonyProlongationRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_prolongation_rule_name(HarmonyProlongationRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_prolongation_valid(const HarmonyTonicProlongation *p) {
    if (p == NULL || !harmony_mode_valid(p->mode)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (!harmony_chord_valid(&p->chords[i]) || !beat_strength_valid(p->beat[i])) {
            return false;
        }
    }
    return true;
}

static int degree(const HarmonyTonicProlongation *p, int chord, int voice) {
    return harmony_step_degree(p->steps[chord][voice]);
}

static int count_degree(const HarmonyTonicProlongation *p, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(p, chord, v) == wanted;
    }
    return count;
}

static bool check_bass_line(const HarmonyTonicProlongation *p) {
    HarmonyProlongationLine line;
    int bass[3];
    if (!harmony_prolongation_of(p->chords, &line) || !harmony_prolongation_line_bass(line, bass)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (degree(p, i, VOICE_PART_BASS) != bass[i]) {
            return false;
        }
    }
    return true;
}

/* Only the three notes of the triad sound, and the root and the third among them. */
static bool triad_notes(const HarmonyTonicProlongation *p, int chord, int root, int third, int fifth) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int d = degree(p, chord, v);
        if (d != root && d != third && d != fifth) {
            return false;
        }
    }
    return count_degree(p, chord, root) > 0 && count_degree(p, chord, third) > 0;
}

static bool seventh_chord_notes(const HarmonyTonicProlongation *p, int chord) {
    return count_degree(p, chord, 5) == 1 && count_degree(p, chord, 7) == 1 && count_degree(p, chord, 2) == 1 &&
           count_degree(p, chord, 4) == 1;
}

static bool check_chord_notes(const HarmonyTonicProlongation *p) {
    HarmonyProlongationLine line;
    if (!harmony_prolongation_of(p->chords, &line)) {
        return false;
    }
    bool middle = p->chords[1].seventh ? seventh_chord_notes(p, 1) : triad_notes(p, 1, 5, 7, 2);
    return triad_notes(p, 0, 1, 3, 5) && middle && triad_notes(p, 2, 1, 3, 5);
}

bool harmony_prolongation_rule_holds(HarmonyProlongationRule which, const HarmonyTonicProlongation *prolongation) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_prolongation_valid(prolongation)) {
        return false;
    }
    HarmonyProlongationLine line;
    switch (which) {
        case HARMONY_PROLONGATION_PROGRESSION:
            return harmony_prolongation_of(prolongation->chords, &line);
        case HARMONY_PROLONGATION_BASS_LINE:
            return check_bass_line(prolongation);
        case HARMONY_PROLONGATION_CHORD_NOTES:
            return check_chord_notes(prolongation);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_prolongation_rule_holds((HarmonyProlongationRule)which, subject);
}

bool harmony_prolongation_check(const HarmonyTonicProlongation *prolongation, unsigned *hard_violations,
                                unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_prolongation_valid(prolongation)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, prolongation, hard_violations, soft_violations);
    return true;
}
