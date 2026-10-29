#include "harmony_resolution_rules.h"

#include "harmony_rule_table.h"
#include "harmony_steps.h"

static const char *const NAMES[HARMONY_RESOLUTION_RULE_COUNT] = {"seventh-resolves-down",
                                                                 "leading-tone-resolves"};

static HarmonyRulePair g_pairs[HARMONY_RESOLUTION_RULE_COUNT];
static UrsatzOnce g_built[HARMONY_RESOLUTION_RULE_COUNT];
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                                    const Context *context);
static const HarmonyRuleTable TABLE = {"resolution", NAMES,   HARMONY_RESOLUTION_RULE_COUNT,
                                       NULL,         g_pairs, g_built, evaluate_candidate};

const Rule *harmony_resolution_rule(HarmonyResolutionRule which) {
    return harmony_rule_table_rule(&TABLE, (int)which);
}

const Constraint *harmony_resolution_constraint(HarmonyResolutionRule which) {
    return harmony_rule_table_constraint(&TABLE, (int)which);
}

const char *harmony_resolution_rule_name(HarmonyResolutionRule which) {
    return harmony_rule_table_name(&TABLE, (int)which);
}

bool harmony_resolution_valid(const HarmonyVoiceResolution *resolution) {
    return resolution != NULL && harmony_mode_valid(resolution->mode) &&
           harmony_chord_valid(&resolution->from) && harmony_chord_valid(&resolution->to);
}

/* The degree of the seventh above a chord's root. */
static int seventh_degree(const HarmonyChord *chord) { return (chord->degree - 1 + 6) % 7 + 1; }

static bool is_seventh_voice(const HarmonyVoiceResolution *r) {
    return r->from.seventh && harmony_step_degree(r->from_step) == seventh_degree(&r->from);
}

/* V, V7, vii-diminished and its seventh chord have the leading tone; in minor the vii is the one on
 * the raised seventh. */
static bool chord_has_leading_tone(const HarmonyVoiceResolution *r) {
    if (r->from.degree == 5) {
        return true;
    }
    return r->from.degree == 7 && (r->mode == HARMONY_MODE_MAJOR || r->from.leading_tone);
}

static bool is_leading_tone_voice(const HarmonyVoiceResolution *r) {
    return chord_has_leading_tone(r) && harmony_step_degree(r->from_step) == 7 && r->to.degree == 1;
}

bool harmony_resolution_rule_applies(HarmonyResolutionRule which,
                                     const HarmonyVoiceResolution *resolution) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_resolution_valid(resolution)) {
        return false;
    }
    return which == HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN ? is_seventh_voice(resolution)
                                                             : is_leading_tone_voice(resolution);
}

bool harmony_resolution_rule_holds(HarmonyResolutionRule which,
                                   const HarmonyVoiceResolution *resolution) {
    if (!harmony_rule_table_valid(&TABLE, (int)which) || !harmony_resolution_valid(resolution)) {
        return false;
    }
    if (!harmony_resolution_rule_applies(which, resolution)) {
        return true;
    }
    int move = resolution->to_step - resolution->from_step;
    if (which == HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN) {
        return move == -1;
    }
    if (move == 1) {
        return true;
    }
    /* the leading-tone drop: an inner voice of a complete V7 goes down a third to sol */
    return !resolution->outer && resolution->complete_dominant_seventh &&
           resolution->from.seventh && resolution->from.degree == 5 && move == -2 &&
           harmony_step_degree(resolution->to_step) == 5;
}

static bool holds_adapter(int which, const void *subject) {
    return harmony_resolution_rule_holds((HarmonyResolutionRule)which, subject);
}

static bool applies_adapter(int which, const void *subject) {
    return harmony_resolution_rule_applies((HarmonyResolutionRule)which, subject);
}

/* A rule that does not apply to the voice (not the seventh, not the leading tone) is NOT_APPLICABLE rather than a
 * vacuous PASS. */
static ConstraintEvalResult evaluate_candidate(const Constraint *constraint, const Value *candidate,
                                               const Context *context) {
    (void)context;
    return harmony_rule_table_evaluate(&TABLE, holds_adapter, applies_adapter, sizeof(HarmonyVoiceResolution),
                                       constraint, candidate);
}
