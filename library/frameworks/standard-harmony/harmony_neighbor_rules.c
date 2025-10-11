#include "harmony_neighbor_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_NEIGHBOR_RULE_COUNT] = {
    "is-six-four", "static-bass", "same-outer-chords", "bass-is-root", "six-four-notes", "upper-neighbors",
    "prolongs-i-or-v"};

static bool table_is_hard(int which) {
    return which <= HARMONY_NEIGHBOR_UPPER_NEIGHBORS;
}

static HarmonyRulePair g_pairs[HARMONY_NEIGHBOR_RULE_COUNT];
static bool g_built[HARMONY_NEIGHBOR_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"neighbor", NAMES, HARMONY_NEIGHBOR_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate};

const Rule *harmony_neighbor_rule(HarmonyNeighborRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_neighbor_constraint(HarmonyNeighborRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_neighbor_rule_name(HarmonyNeighborRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

static bool check_static_bass(const HarmonySixFour *s) {
    return s->steps[0][VOICE_PART_BASS] == s->steps[1][VOICE_PART_BASS] &&
           s->steps[1][VOICE_PART_BASS] == s->steps[2][VOICE_PART_BASS];
}

static bool check_same_outer_chords(const HarmonySixFour *s) {
    const HarmonyChord *a = &s->chords[0];
    const HarmonyChord *b = &s->chords[2];
    return a->degree == b->degree && a->inversion == 0 && b->inversion == 0 && !a->seventh && !b->seventh;
}

static bool check_bass_is_root(const HarmonySixFour *s) {
    return s->chords[0].degree == harmony_step_degree(s->steps[1][VOICE_PART_BASS]);
}

static int count_degree(const HarmonySixFour *s, int chord, int wanted) {
    int count = 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        count += harmony_step_degree(s->steps[chord][v]) == wanted;
    }
    return count;
}

static bool check_six_four_notes(const HarmonySixFour *s) {
    int bass = harmony_step_degree(s->steps[1][VOICE_PART_BASS]);
    int fourth = (bass - 1 + 3) % 7 + 1;
    int sixth = (bass - 1 + 5) % 7 + 1;
    return count_degree(s, 1, bass) == 2 && count_degree(s, 1, fourth) == 1 && count_degree(s, 1, sixth) == 1;
}

/* Two upper voices up a step and back, and the other upper voice on the same note. */
static bool check_upper_neighbors(const HarmonySixFour *s) {
    int neighbors = 0, still = 0;
    for (int v = 0; v < VOICE_PART_BASS; v++) {
        int a = s->steps[0][v], b = s->steps[1][v], c = s->steps[2][v];
        if (b == a + 1 && c == a) {
            neighbors++;
        } else if (b == a && c == a) {
            still++;
        }
    }
    return neighbors == 2 && still == 1;
}

bool harmony_neighbor_rule_holds(HarmonyNeighborRule which, const HarmonySixFour *six_four) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_six_four_valid(six_four)) {
        return false;
    }
    switch (which) {
        case HARMONY_NEIGHBOR_IS_SIX_FOUR:
            return harmony_six_four_middle_is_six_four(six_four);
        case HARMONY_NEIGHBOR_STATIC_BASS:
            return check_static_bass(six_four);
        case HARMONY_NEIGHBOR_SAME_OUTER_CHORDS:
            return check_same_outer_chords(six_four);
        case HARMONY_NEIGHBOR_BASS_IS_ROOT:
            return check_bass_is_root(six_four);
        case HARMONY_NEIGHBOR_SIX_FOUR_NOTES:
            return check_six_four_notes(six_four);
        case HARMONY_NEIGHBOR_UPPER_NEIGHBORS:
            return check_upper_neighbors(six_four);
        case HARMONY_NEIGHBOR_PROLONGS_I_OR_V:
            return six_four->chords[0].degree == 1 || six_four->chords[0].degree == 5;
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_neighbor_rule_holds((HarmonyNeighborRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonySixFour)

bool harmony_neighbor_check(const HarmonySixFour *six_four, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_six_four_valid(six_four)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, six_four, hard_violations, soft_violations);
    return true;
}
