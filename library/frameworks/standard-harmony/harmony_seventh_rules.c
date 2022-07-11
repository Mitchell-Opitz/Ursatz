#include "harmony_seventh_rules.h"

#include "harmony_rule_table.h"

#define SOL 5
#define TI 7
#define RE 2
#define FA 4
#define DO 1
#define MI 3

static const char *const NAMES[HARMONY_SEVENTH_RULE_COUNT] = {"bass-sol-to-do",     "v7-notes",
                                                              "active-notes-resolve", "tonic-notes",
                                                              "minor-leading-tone",  "avoid-re-to-mi"};

static bool table_is_hard(int which) {
    return which <= HARMONY_SEVENTH_MINOR_LEADING_TONE;
}

static HarmonyRulePair g_pairs[HARMONY_SEVENTH_RULE_COUNT];
static bool g_built[HARMONY_SEVENTH_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"seventh", NAMES, HARMONY_SEVENTH_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *harmony_seventh_rule(HarmonySeventhRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_seventh_constraint(HarmonySeventhRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_seventh_rule_name(HarmonySeventhRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool degree_valid(int degree) {
    return degree >= 1 && degree <= 7;
}

bool harmony_seventh_valid(const HarmonySeventhResolution *resolution) {
    if (resolution == NULL || !harmony_mode_valid(resolution->mode)) {
        return false;
    }
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (!degree_valid(resolution->dominant[v]) || !degree_valid(resolution->tonic[v])) {
            return false;
        }
    }
    return true;
}

static int count_of(const int *chord, int degree) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += chord[v] == degree;
    }
    return count;
}

static bool check_v7_notes(const HarmonySeventhResolution *r) {
    return count_of(r->dominant, SOL) == 1 && count_of(r->dominant, TI) == 1 && count_of(r->dominant, RE) == 1 &&
           count_of(r->dominant, FA) == 1;
}

static bool check_active_notes_resolve(const HarmonySeventhResolution *r) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int to = r->tonic[v];
        switch (r->dominant[v]) {
            case FA:
                if (to != MI) {
                    return false;
                }
                break;
            case TI:
                if (to != DO) {
                    return false;
                }
                break;
            case RE:
                if (to != DO && to != MI) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    return true;
}

static bool check_tonic_notes(const HarmonySeventhResolution *r) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (r->tonic[v] != DO && r->tonic[v] != MI && r->tonic[v] != SOL) {
            return false;
        }
    }
    return count_of(r->tonic, DO) > 0 && count_of(r->tonic, MI) > 0;
}

static bool check_avoid_re_to_mi(const HarmonySeventhResolution *r) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (r->dominant[v] == RE && r->tonic[v] == MI) {
            return false;
        }
    }
    return true;
}

bool harmony_seventh_rule_holds(HarmonySeventhRule which, const HarmonySeventhResolution *resolution) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_seventh_valid(resolution)) {
        return false;
    }
    switch (which) {
        case HARMONY_SEVENTH_BASS_SOL_TO_DO:
            return resolution->dominant[VOICE_PART_BASS] == SOL && resolution->tonic[VOICE_PART_BASS] == DO;
        case HARMONY_SEVENTH_V7_NOTES:
            return check_v7_notes(resolution);
        case HARMONY_SEVENTH_ACTIVE_NOTES_RESOLVE:
            return check_active_notes_resolve(resolution);
        case HARMONY_SEVENTH_TONIC_NOTES:
            return check_tonic_notes(resolution);
        case HARMONY_SEVENTH_MINOR_LEADING_TONE:
            return resolution->mode == HARMONY_MODE_MAJOR || resolution->dominant_has_leading_tone;
        case HARMONY_SEVENTH_AVOID_RE_TO_MI:
            return check_avoid_re_to_mi(resolution);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_seventh_rule_holds((HarmonySeventhRule)which, subject);
}

bool harmony_seventh_check(const HarmonySeventhResolution *resolution, unsigned *hard_violations,
                           unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_seventh_valid(resolution)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, resolution, hard_violations, soft_violations);
    return true;
}
