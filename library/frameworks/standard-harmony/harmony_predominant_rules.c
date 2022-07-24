#include "harmony_predominant_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"
#include "harmony_strong_predominants.h"

static const char *const NAMES[HARMONY_PREDOMINANT_RULE_COUNT] = {
    "strong-predominant", "bass-motion",        "chord-notes",
    "active-note-resolves", "minor-leading-tone", "no-parallels",
    "not-all-voices-same-direction", "soprano-approach", "iv-doubles-bass",
    "ii-soprano",         "root-ii-major-only"};

static bool table_is_hard(int which) {
    return which <= HARMONY_PREDOMINANT_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_PREDOMINANT_RULE_COUNT];
static bool g_built[HARMONY_PREDOMINANT_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"predominant", NAMES, HARMONY_PREDOMINANT_RULE_COUNT, table_is_hard,
                                       g_pairs, g_built};

const Rule *harmony_predominant_rule(HarmonyPredominantRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_predominant_constraint(HarmonyPredominantRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_predominant_rule_name(HarmonyPredominantRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_predominant_ending_valid(const HarmonyPredominantEnding *ending) {
    return ending != NULL && harmony_mode_valid(ending->mode) && harmony_chord_valid(&ending->predominant);
}

static bool kind_of(const HarmonyPredominantEnding *e, HarmonyStrongPredominant *kind) {
    return harmony_strong_predominant_of(e->mode, &e->predominant, kind);
}

/* The degree of the voice's step. */
static int degree(const int *steps, int voice) {
    return harmony_step_degree(steps[voice]);
}

static int count_degree(const int *steps, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(steps, v) == wanted;
    }
    return count;
}

/* Only the three notes of the triad sound, and the root and the third among them. */
static bool chord_notes(const int *steps, int root, int third, int fifth) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int d = degree(steps, v);
        if (d != root && d != third && d != fifth) {
            return false;
        }
    }
    return count_degree(steps, root) > 0 && count_degree(steps, third) > 0;
}

static bool check_bass_motion(const HarmonyPredominantEnding *e) {
    HarmonyStrongPredominant kind;
    if (!kind_of(e, &kind)) {
        return false;
    }
    int predominant_bass = kind == HARMONY_STRONG_PREDOMINANT_II ? 2 : 4;
    return degree(e->predominant_steps, VOICE_PART_BASS) == predominant_bass &&
           degree(e->dominant_steps, VOICE_PART_BASS) == 5 &&
           (!e->has_tonic || degree(e->tonic_steps, VOICE_PART_BASS) == 1);
}

static bool check_chord_notes(const HarmonyPredominantEnding *e) {
    HarmonyStrongPredominant kind;
    if (!kind_of(e, &kind)) {
        return false;
    }
    bool predominant = kind == HARMONY_STRONG_PREDOMINANT_IV ? chord_notes(e->predominant_steps, 4, 6, 1)
                                                             : chord_notes(e->predominant_steps, 2, 4, 6);
    return predominant && chord_notes(e->dominant_steps, 5, 7, 2) &&
           (!e->has_tonic || chord_notes(e->tonic_steps, 1, 3, 5));
}

static bool check_active_note_resolves(const HarmonyPredominantEnding *e) {
    if (!e->has_tonic) {
        return true;
    }
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int d = degree(e->dominant_steps, v);
        if ((d == 7 && e->tonic_steps[v] != e->dominant_steps[v] + 1) ||
            (d == 2 && e->tonic_steps[v] != e->dominant_steps[v] - 1)) {
            return false;
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyPredominantEnding *e) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            if (harmony_steps_parallel(e->predominant_steps[a], e->dominant_steps[a], e->predominant_steps[b],
                                       e->dominant_steps[b])) {
                return false;
            }
            if (e->has_tonic && harmony_steps_parallel(e->dominant_steps[a], e->tonic_steps[a],
                                                       e->dominant_steps[b], e->tonic_steps[b])) {
                return false;
            }
        }
    }
    return true;
}

static bool check_not_all_same_direction(const HarmonyPredominantEnding *e) {
    int first = harmony_step_direction(e->predominant_steps[0], e->dominant_steps[0]);
    if (first == 0) {
        return true;
    }
    for (int v = 1; v < VOICE_PART_COUNT; v++) {
        if (harmony_step_direction(e->predominant_steps[v], e->dominant_steps[v]) != first) {
            return true;
        }
    }
    return false;
}

bool harmony_predominant_rule_holds(HarmonyPredominantRule which, const HarmonyPredominantEnding *ending) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_predominant_ending_valid(ending)) {
        return false;
    }
    HarmonyStrongPredominant kind;
    bool is_strong = kind_of(ending, &kind);
    switch (which) {
        case HARMONY_PREDOMINANT_STRONG_PREDOMINANT:
            return is_strong;
        case HARMONY_PREDOMINANT_BASS_MOTION:
            return check_bass_motion(ending);
        case HARMONY_PREDOMINANT_CHORD_NOTES:
            return check_chord_notes(ending);
        case HARMONY_PREDOMINANT_ACTIVE_NOTE_RESOLVES:
            return check_active_note_resolves(ending);
        case HARMONY_PREDOMINANT_MINOR_LEADING_TONE:
            return ending->mode == HARMONY_MODE_MAJOR || ending->dominant_has_leading_tone;
        case HARMONY_PREDOMINANT_NO_PARALLELS:
            return check_no_parallels(ending);
        case HARMONY_PREDOMINANT_NOT_ALL_VOICES_SAME_DIRECTION:
            return check_not_all_same_direction(ending);
        case HARMONY_PREDOMINANT_SOPRANO_APPROACH:
            return ending->predominant_steps[VOICE_PART_SOPRANO] >= ending->dominant_steps[VOICE_PART_SOPRANO];
        case HARMONY_PREDOMINANT_IV_DOUBLES_BASS:
            return !is_strong || kind != HARMONY_STRONG_PREDOMINANT_IV ||
                   count_degree(ending->predominant_steps, 4) >= 2;
        case HARMONY_PREDOMINANT_II_SOPRANO: {
            int soprano = degree(ending->predominant_steps, VOICE_PART_SOPRANO);
            return !is_strong || kind != HARMONY_STRONG_PREDOMINANT_II || soprano == 2 || soprano == 4;
        }
        case HARMONY_PREDOMINANT_ROOT_II_MAJOR_ONLY:
            return !is_strong || kind != HARMONY_STRONG_PREDOMINANT_II || ending->mode == HARMONY_MODE_MAJOR;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_predominant_rule_holds((HarmonyPredominantRule)which, subject);
}

bool harmony_predominant_check(const HarmonyPredominantEnding *ending, unsigned *hard_violations,
                               unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_predominant_ending_valid(ending)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, ending, hard_violations, soft_violations);
    return true;
}
