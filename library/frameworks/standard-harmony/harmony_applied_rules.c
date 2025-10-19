#include "harmony_applied_rules.h"

#include <stddef.h>

#include "harmony_rule_table.h"

static const char *const NAMES[HARMONY_APPLIED_RULE_COUNT] = {
    "progression",   "chord-notes", "leading-tone-resolves",  "seventh-resolves",
    "no-parallels",  "avoid-half-diminished", "chromatic"};

static bool table_is_hard(int which) {
    return which <= HARMONY_APPLIED_NO_PARALLELS;
}

static HarmonyRulePair g_pairs[HARMONY_APPLIED_RULE_COUNT];
static bool g_built[HARMONY_APPLIED_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static PreferenceScore score_candidate(const Preference *preference, const Value *candidate, const Context *context);
static const HarmonyRuleTable TABLE = {"applied", NAMES, HARMONY_APPLIED_RULE_COUNT, table_is_hard, g_pairs, g_built,
                                       evaluate_candidate, score_candidate};

const Rule *harmony_applied_rule(HarmonyAppliedRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_applied_constraint(HarmonyAppliedRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const Preference *harmony_applied_preference(HarmonyAppliedRule which) {
    return harmony_rule_table_preference(&TABLE, (int)which);
}

const char *harmony_applied_rule_name(HarmonyAppliedRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_applied_motion_valid(const HarmonyAppliedMotion *m) {
    return m != NULL && harmony_applied_valid(m->mode, &m->applied) && harmony_chord_valid(&m->next);
}

int harmony_applied_pitch(HarmonyMode mode, int step, int alter) {
    int octave = step >= 0 ? step / 7 : -((-step + 6) / 7);
    int degree = step - 7 * octave + 1;
    return 12 * octave + harmony_applied_scale_semitone(mode, degree) + alter;
}

static int pitch(const HarmonyAppliedMotion *m, int chord, int voice) {
    return harmony_applied_pitch(m->mode, m->steps[chord][voice], m->alter[chord][voice]);
}

/* The pitch class of a voice above the applied chord's root: 0 to 11. */
static int above_root(const HarmonyAppliedMotion *m, int voice) {
    int root = harmony_applied_root_semitone(m->mode, &m->applied);
    return (((pitch(m, 0, voice) - root) % 12) + 12) % 12;
}

static bool check_chord_notes(const HarmonyAppliedMotion *m) {
    HarmonyAppliedQuality q = m->applied.quality;
    bool dominant = q == HARMONY_APPLIED_MAJOR_TRIAD || q == HARMONY_APPLIED_DOMINANT_SEVENTH;
    int third = dominant ? 4 : 3;
    int fifth = dominant ? 7 : 6;
    int seventh = q == HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH ? 9 : 10;
    bool seventh_chord = harmony_applied_quality_is_seventh(q);
    bool has_root = false, has_third = false, has_seventh = false;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        int pc = above_root(m, v);
        if (pc == 0) {
            has_root = true;
        } else if (pc == third) {
            has_third = true;
        } else if (seventh_chord && pc == seventh) {
            has_seventh = true;
        } else if (pc != fifth) {
            return false;
        }
    }
    return has_root && has_third && (!seventh_chord || has_seventh);
}

static int move(const HarmonyAppliedMotion *m, int voice) {
    return pitch(m, 1, voice) - pitch(m, 0, voice);
}

static bool check_leading_tone_resolves(const HarmonyAppliedMotion *m) {
    int leading = m->applied.kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT ? 4 : 0;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (above_root(m, v) != leading) {
            continue;
        }
        int step = move(m, v);
        if (step != 1 && !(step == -1 && m->next.seventh)) {
            return false;
        }
    }
    return true;
}

static bool check_seventh_resolves(const HarmonyAppliedMotion *m) {
    if (!harmony_applied_quality_is_seventh(m->applied.quality)) {
        return true;
    }
    int seventh = m->applied.quality == HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH ? 9 : 10;
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (above_root(m, v) == seventh && (move(m, v) < -2 || move(m, v) > -1)) {
            return false;
        }
    }
    return true;
}

/* 0 for an octave or unison, 7 for a fifth, otherwise -1, measured in pitch classes. */
static int perfect(int low, int high) {
    int interval = (((high - low) % 12) + 12) % 12;
    return interval == 0 || interval == 7 ? interval : -1;
}

static bool check_no_parallels(const HarmonyAppliedMotion *m) {
    for (int a = 0; a < VOICE_PART_COUNT; a++) {
        for (int b = a + 1; b < VOICE_PART_COUNT; b++) {
            int ma = move(m, a), mb = move(m, b);
            if (ma == 0 || mb == 0 || (ma > 0) != (mb > 0)) {
                continue;
            }
            int before = perfect(pitch(m, 0, b), pitch(m, 0, a));
            int after = perfect(pitch(m, 1, b), pitch(m, 1, a));
            if (before >= 0 && before == after) {
                return false;
            }
        }
    }
    return true;
}

static bool check_chromatic(const HarmonyAppliedMotion *m) {
    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        if (m->alter[0][v] != 0) {
            return true;
        }
    }
    return false;
}

bool harmony_applied_rule_holds(HarmonyAppliedRule which, const HarmonyAppliedMotion *motion) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_applied_motion_valid(motion)) {
        return false;
    }
    switch (which) {
        case HARMONY_APPLIED_PROGRESSION:
            return motion->next.degree == motion->applied.target ||
                   (motion->applied.target == 5 && motion->next.cadential);
        case HARMONY_APPLIED_CHORD_NOTES:
            return check_chord_notes(motion);
        case HARMONY_APPLIED_LEADING_TONE_RESOLVES:
            return check_leading_tone_resolves(motion);
        case HARMONY_APPLIED_SEVENTH_RESOLVES:
            return check_seventh_resolves(motion);
        case HARMONY_APPLIED_NO_PARALLELS:
            return check_no_parallels(motion);
        case HARMONY_APPLIED_AVOID_HALF_DIMINISHED:
            return motion->applied.quality != HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH;
        case HARMONY_APPLIED_CHROMATIC:
            return check_chromatic(motion);
        default:
            return false;
    }
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_applied_rule_holds((HarmonyAppliedRule)which, subject);
}

HARMONY_RULE_TABLE_EVALUATOR(evaluate_candidate, TABLE, holds_adapter, HarmonyAppliedMotion)
HARMONY_RULE_TABLE_SCORER(score_candidate, TABLE, holds_adapter, HarmonyAppliedMotion)

bool harmony_applied_check(const HarmonyAppliedMotion *motion, unsigned *hard_violations,
                           unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !harmony_applied_motion_valid(motion)) {
        return false;
    }
    harmony_rule_table_check(&TABLE, holds_adapter, motion, hard_violations, soft_violations);
    return true;
}
