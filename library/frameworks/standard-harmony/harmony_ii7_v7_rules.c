#include "harmony_ii7_v7_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_II7_V7_RULE_COUNT] = {"progression", "chord-notes", "one-incomplete",
                                                             "no-parallels"};

static HarmonyRulePair g_pairs[HARMONY_II7_V7_RULE_COUNT];
static bool g_built[HARMONY_II7_V7_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"ii7-v7", NAMES, HARMONY_II7_V7_RULE_COUNT, NULL, g_pairs, g_built,
                                       evaluate_candidate};

const Rule *harmony_ii7_v7_rule(HarmonyIi7V7Rule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_ii7_v7_constraint(HarmonyIi7V7Rule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_ii7_v7_rule_name(HarmonyIi7V7Rule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool is_seventh(const HarmonyChord *chord, int degree) {
    return chord->degree == degree && chord->inversion == 0 && chord->seventh && !chord->cadential;
}

bool harmony_ii7_v7_progression_is(const HarmonyChord chords[2]) {
    if (chords == NULL || !harmony_chord_valid(&chords[0]) || !harmony_chord_valid(&chords[1])) {
        return false;
    }
    return is_seventh(&chords[0], 2) && is_seventh(&chords[1], 5);
}

bool harmony_ii7_v7_valid(const HarmonyIi7V7 *s) {
    return s != NULL && harmony_mode_valid(s->mode) && harmony_chord_valid(&s->chords[0]) &&
           harmony_chord_valid(&s->chords[1]);
}

static int degree(const HarmonyIi7V7 *s, int chord, int voice) {
    return harmony_step_degree(s->steps[chord][voice]);
}

static int count_degree(const HarmonyIi7V7 *s, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += degree(s, chord, v) == wanted;
    }
    return count;
}

/* A seventh chord on `root`: only its notes, its root, third, and seventh present, the root in
   the bass. */
static bool chord_notes(const HarmonyIi7V7 *s, int chord, int root) {
    int third = (root - 1 + 2) % 7 + 1;
    int fifth = (root - 1 + 4) % 7 + 1;
    int seventh = (root - 1 + 6) % 7 + 1;
    int present = count_degree(s, chord, root) + count_degree(s, chord, third) + count_degree(s, chord, fifth) +
                  count_degree(s, chord, seventh);
    return present == VOICE_PART_COUNT && count_degree(s, chord, root) > 0 && count_degree(s, chord, third) > 0 &&
           count_degree(s, chord, seventh) > 0 && degree(s, chord, VOICE_PART_BASS) == root;
}

static bool check_one_incomplete(const HarmonyIi7V7 *s) {
    bool ii7 = count_degree(s, 0, 2) == 2 && count_degree(s, 0, 6) == 0;
    bool v7 = count_degree(s, 1, 5) == 2 && count_degree(s, 1, 2) == 0;
    return ii7 || v7;
}

static bool check_no_parallels(const HarmonyIi7V7 *s) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            if (harmony_steps_parallel(s->steps[0][a], s->steps[1][a], s->steps[0][b], s->steps[1][b])) {
                return false;
            }
        }
    }
    return true;
}

bool harmony_ii7_v7_rule_holds(HarmonyIi7V7Rule which, const HarmonyIi7V7 *subject) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_ii7_v7_valid(subject)) {
        return false;
    }
    bool classified = harmony_ii7_v7_progression_is(subject->chords);
    switch (which) {
        case HARMONY_II7_V7_PROGRESSION:
            return classified;
        case HARMONY_II7_V7_CHORD_NOTES:
            return classified && chord_notes(subject, 0, 2) && chord_notes(subject, 1, 5);
        case HARMONY_II7_V7_ONE_INCOMPLETE:
            return check_one_incomplete(subject);
        case HARMONY_II7_V7_NO_PARALLELS:
            return check_no_parallels(subject);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_ii7_v7_rule_holds((HarmonyIi7V7Rule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyIi7V7)

bool harmony_ii7_v7_check(const HarmonyIi7V7 *subject, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_ii7_v7_valid(subject)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, subject, hard_violations, soft_violations);
    return true;
}
