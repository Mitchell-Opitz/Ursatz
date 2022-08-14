#include "harmony_prolongation_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_PROLONGATION_RULE_COUNT] = {"progression",        "bass-line",    "chord-notes",
                                                                  "resolves",           "minor-leading-tone", "no-parallels",
                                                                  "avoid-v6-triad",     "avoid-v43-to-root",  "middle-weaker-beat"};

static bool table_is_hard(int which) {
    return which <= HARMONY_PROLONGATION_NO_PARALLELS;
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

/* The upper voice a tenth above the bass in all three chords: parallel tenths. */
static bool tenths_with_bass(const HarmonyTonicProlongation *p, int voice) {
    for (int i = 0; i < 3; i++) {
        if (p->steps[i][voice] - p->steps[i][VOICE_PART_BASS] != 9) {
            return false;
        }
    }
    return true;
}

static bool check_resolves(const HarmonyTonicProlongation *p) {
    HarmonyProlongationLine line = HARMONY_PROLONGATION_LINE_COUNT;
    bool classified = harmony_prolongation_of(p->chords, &line);
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int move = p->steps[2][v] - p->steps[1][v];
        switch (degree(p, 1, v)) {
            case 7: /* ti up a step to do */
                if (move != 1) {
                    return false;
                }
                break;
            case 4: /* the chordal seventh down a step to mi, or up to sol in parallel tenths */
                if (move != -1 && !(move == 1 && classified && line == HARMONY_PROLONGATION_DO_RE_MI &&
                                    v != VOICE_PART_BASS && tenths_with_bass(p, v))) {
                    return false;
                }
                break;
            case 2: /* re a step, to do or mi */
                if (move != 1 && move != -1) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyTonicProlongation *p) {
    for (int i = 0; i < 2; i++) {
        for (int a = 0; a < VOICE_PART_COUNT; a++) {
            for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
                if (harmony_steps_parallel(p->steps[i][a], p->steps[i + 1][a], p->steps[i][b], p->steps[i + 1][b])) {
                    return false;
                }
            }
        }
    }
    return true;
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
        case HARMONY_PROLONGATION_RESOLVES:
            return check_resolves(prolongation);
        case HARMONY_PROLONGATION_MINOR_LEADING_TONE:
            return prolongation->mode == HARMONY_MODE_MAJOR || prolongation->dominant_has_leading_tone;
        case HARMONY_PROLONGATION_NO_PARALLELS:
            return check_no_parallels(prolongation);
        case HARMONY_PROLONGATION_AVOID_V6_TRIAD:
            return prolongation->chords[1].seventh;
        case HARMONY_PROLONGATION_AVOID_V43_TO_ROOT:
            return !(harmony_prolongation_of(prolongation->chords, &line) &&
                     line == HARMONY_PROLONGATION_DO_RE_DO);
        case HARMONY_PROLONGATION_MIDDLE_WEAKER_BEAT:
            return beat_strength_is_stronger(prolongation->beat[0], prolongation->beat[1]) &&
                   beat_strength_is_stronger(prolongation->beat[2], prolongation->beat[1]);
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
