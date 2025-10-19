#include "harmony_connection_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

#define CHORD_COUNT 3

static const char *const NAMES[HARMONY_CONNECTION_RULE_COUNT] = {"progression", "bass-line", "chord-notes",
                                                                 "step-or-common-tone", "no-parallels"};

static HarmonyRulePair g_pairs[HARMONY_CONNECTION_RULE_COUNT];
static bool g_built[HARMONY_CONNECTION_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"connection", NAMES, HARMONY_CONNECTION_RULE_COUNT, NULL, g_pairs, g_built,
                                       evaluate_candidate, score_candidate};

const Rule *harmony_connection_rule(HarmonyConnectionRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_connection_constraint(HarmonyConnectionRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_connection_preference(HarmonyConnectionRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_connection_rule_name(HarmonyConnectionRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool is_triad(const HarmonyChord *chord, int degree, int inversion) {
    return chord->degree == degree && chord->inversion == inversion && !chord->seventh && !chord->cadential;
}

bool harmony_connection_progression_is(const HarmonyChord chords[3]) {
    if (chords == NULL) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!harmony_chord_valid(&chords[i])) {
            return false;
        }
    }
    return is_triad(&chords[0], 1, 0) && (is_triad(&chords[1], 6, 0) || is_triad(&chords[1], 4, 1)) &&
           (is_triad(&chords[2], 4, 0) || is_triad(&chords[2], 2, 1));
}

bool harmony_connection_valid(const HarmonyConnection *c) {
    if (c == NULL || !harmony_mode_valid(c->mode)) {
        return false;
    }
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (!harmony_chord_valid(&c->chords[i])) {
            return false;
        }
    }
    return true;
}

static int degree(const HarmonyConnection *c, int chord, int voice) {
    return harmony_step_degree(c->steps[chord][voice]);
}

static int count_degree(const HarmonyConnection *c, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(c, chord, v) == wanted;
    }
    return count;
}

static bool check_bass_line(const HarmonyConnection *c) {
    static const int line[CHORD_COUNT] = {1, 6, 4};
    for (int i = 0; i < CHORD_COUNT; i++) {
        if (degree(c, i, VOICE_PART_BASS) != line[i]) {
            return false;
        }
    }
    for (int i = 0; i + 1 < CHORD_COUNT; i++) {
        if (c->steps[i + 1][VOICE_PART_BASS] - c->steps[i][VOICE_PART_BASS] > -2) {
            return false;
        }
    }
    return true;
}

/* Each chord has only its own notes, its root and third present. The bass of the line is
   the root of I, vi, and IV, and the third of IV6 and ii6. */
static bool check_chord_notes(const HarmonyConnection *c) {
    for (int i = 0; i < CHORD_COUNT; i++) {
        int root = c->chords[i].degree;
        int third = (root - 1 + 2) % 7 + 1;
        int fifth = (root - 1 + 4) % 7 + 1;
        int present = count_degree(c, i, root) + count_degree(c, i, third) + count_degree(c, i, fifth);
        if (present != VOICE_PART_COUNT || count_degree(c, i, root) == 0 || count_degree(c, i, third) == 0) {
            return false;
        }
        int bass = c->chords[i].inversion == 0 ? root : third;
        if (degree(c, i, VOICE_PART_BASS) != bass) {
            return false;
        }
    }
    return true;
}

static bool check_step_or_common_tone(const HarmonyConnection *c) {
    for (int k = 0; k + 1 < CHORD_COUNT; k++) {
        for (int v = 0; v < VOICE_PART_BASS; v++) {
            int move = c->steps[k + 1][v] - c->steps[k][v];
            if (move < -1 || move > 1) {
                return false;
            }
        }
    }
    return true;
}

static bool check_no_parallels(const HarmonyConnection *c) {
    for (int k = 0; k + 1 < CHORD_COUNT; k++) {
        for (int a = 0; a < VOICE_PART_COUNT; a++) {
            for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
                if (harmony_steps_parallel(c->steps[k][a], c->steps[k + 1][a], c->steps[k][b], c->steps[k + 1][b])) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool harmony_connection_rule_holds(HarmonyConnectionRule which, const HarmonyConnection *connection) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_connection_valid(connection)) {
        return false;
    }
    switch (which) {
        case HARMONY_CONNECTION_PROGRESSION:
            return harmony_connection_progression_is(connection->chords);
        case HARMONY_CONNECTION_BASS_LINE:
            return check_bass_line(connection);
        case HARMONY_CONNECTION_CHORD_NOTES:
            return check_chord_notes(connection);
        case HARMONY_CONNECTION_STEP_OR_COMMON_TONE:
            return check_step_or_common_tone(connection);
        case HARMONY_CONNECTION_NO_PARALLELS:
            return check_no_parallels(connection);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_connection_rule_holds((HarmonyConnectionRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyConnection)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyConnection)

bool harmony_connection_check(const HarmonyConnection *connection, unsigned *hard_violations,
                              unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_connection_valid(connection)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, connection, hard_violations, soft_violations);
    return true;
}
