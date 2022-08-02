#include "harmony_embellishing_rules.h"

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_EMBELLISHING_RULE_COUNT] = {
    "classifiable",         "stronger-beat",         "chord-tone-ends",       "middle-not-chord-tone",
    "appoggiatura-strong-beat", "escape-tone-weak-beat", "anticipation-weak-beat", "left-downward"};

static bool table_is_hard(int which) {
    return which <= HARMONY_EMBELLISHING_STRONGER_BEAT;
}

static HarmonyRulePair g_pairs[HARMONY_EMBELLISHING_RULE_COUNT];
static bool g_built[HARMONY_EMBELLISHING_RULE_COUNT];
static const HarmonyRuleTable TABLE = {"embellishing", NAMES, HARMONY_EMBELLISHING_RULE_COUNT, table_is_hard,
                                       g_pairs, g_built};

const Rule *harmony_embellishing_rule(HarmonyEmbellishingRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_embellishing_constraint(HarmonyEmbellishingRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_embellishing_rule_name(HarmonyEmbellishingRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

/* Index of the embellishing note: the middle of three, the first of two. */
static int embellishing_index(const HarmonyGesture *g) {
    return g->note_count == 3 ? 1 : 0;
}

static bool stronger_than_neighbors(const HarmonyGesture *g) {
    return beat_strength_is_stronger(g->beat[1], g->beat[0]) && beat_strength_is_stronger(g->beat[1], g->beat[2]);
}

static bool weaker_than_neighbors(const HarmonyGesture *g) {
    return beat_strength_is_stronger(g->beat[0], g->beat[1]) && beat_strength_is_stronger(g->beat[2], g->beat[1]);
}

static bool check_chord_tone_ends(const HarmonyGesture *g) {
    return g->note_count == 3 ? g->in_chord[0] && g->in_chord[2] : g->in_chord[1];
}

bool harmony_embellishing_rule_holds(HarmonyEmbellishingRule which, const HarmonyGesture *gesture) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_gesture_valid(gesture)) {
        return false;
    }
    NonChordTone tone = NON_CHORD_TONE_COUNT;
    bool classified = harmony_embellishing_classify(gesture, &tone);
    switch (which) {
        case HARMONY_EMBELLISHING_CLASSIFIABLE:
            return classified;
        case HARMONY_EMBELLISHING_STRONGER_BEAT:
            return !classified || (tone != NON_CHORD_TONE_SUSPENSION && tone != NON_CHORD_TONE_RETARDATION) ||
                   stronger_than_neighbors(gesture);
        case HARMONY_EMBELLISHING_CHORD_TONE_ENDS:
            return check_chord_tone_ends(gesture);
        case HARMONY_EMBELLISHING_MIDDLE_NOT_CHORD_TONE:
            return !gesture->in_chord[embellishing_index(gesture)];
        case HARMONY_EMBELLISHING_APPOGGIATURA_STRONG_BEAT:
            return !classified || tone != NON_CHORD_TONE_APPOGGIATURA || stronger_than_neighbors(gesture);
        case HARMONY_EMBELLISHING_ESCAPE_TONE_WEAK_BEAT:
            return !classified || tone != NON_CHORD_TONE_ESCAPE_TONE || weaker_than_neighbors(gesture);
        case HARMONY_EMBELLISHING_ANTICIPATION_WEAK_BEAT:
            return !classified || tone != NON_CHORD_TONE_ANTICIPATION ||
                   beat_strength_is_stronger(gesture->beat[1], gesture->beat[0]);
        case HARMONY_EMBELLISHING_LEFT_DOWNWARD:
            return !classified || (tone != NON_CHORD_TONE_APPOGGIATURA && tone != NON_CHORD_TONE_ESCAPE_TONE) ||
                   gesture->steps[2] < gesture->steps[1];
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_embellishing_rule_holds((HarmonyEmbellishingRule)which, subject);
}

bool harmony_embellishing_check(const HarmonyGesture *gesture, unsigned *hard_violations,
                                unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_gesture_valid(gesture)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, gesture, hard_violations, soft_violations);
    return true;
}
