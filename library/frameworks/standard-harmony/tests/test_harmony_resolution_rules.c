#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_resolution_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "resolution", HARMONY_RESOLUTION_RULE_COUNT);
#define harmony_resolution_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS_APPLIES(g_eval, harmony_resolution_constraint, harmony_eval_no_preference, harmony_resolution_rule_holds, harmony_resolution_rule_applies, HarmonyVoiceResolution, which, subject)

#define R(name) HARMONY_RESOLUTION_##name

static HarmonyVoiceResolution move(HarmonyChord from, HarmonyChord to, int from_step, int to_step, bool outer) {
    HarmonyVoiceResolution r;
    r.mode = HARMONY_MODE_MAJOR;
    r.from = from;
    r.to = to;
    r.from_step = from_step;
    r.to_step = to_step;
    r.outer = outer;
    r.complete_dominant_seventh = false;
    return r;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_resolution_rule, harmony_resolution_constraint, HARMONY_RESOLUTION_RULE_COUNT);
    for (int i = 0; i < HARMONY_RESOLUTION_RULE_COUNT; i++) {
        assert(constraint_type(harmony_resolution_constraint((HarmonyResolutionRule)i)) == CONSTRAINT_TYPE_HARD);
    }
    assert(strcmp(harmony_resolution_rule_name(R(SEVENTH_RESOLVES_DOWN)), "seventh-resolves-down") == 0);
    assert(strcmp(harmony_resolution_rule_name(R(LEADING_TONE_RESOLVES)), "leading-tone-resolves") == 0);
    assert(harmony_resolution_rule_name(R(RULE_COUNT)) == NULL);
}

static void test_seventh_resolves_down_by_step(void) {
    HarmonyChord v7 = CH7(5);
    HarmonyChord i = CH(1);
    HarmonyVoiceResolution ok = move(v7, i, 3, 2, true); /* fa to mi */
    assert(harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), &ok));
    assert(harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &ok));

    HarmonyVoiceResolution held = move(v7, i, 3, 3, true); /* held over: not allowed */
    assert(harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), &held));
    assert(!harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &held));
    HarmonyVoiceResolution up = move(v7, i, 3, 4, true);
    assert(!harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &up));
    HarmonyVoiceResolution leap = move(v7, i, 3, 1, true);
    assert(!harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &leap));

    /* a voice that is not the seventh: the rule does not apply and holds */
    HarmonyVoiceResolution root = move(v7, i, 4, 0, true);
    assert(!harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), &root));
    assert(harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &root));
    /* a chord with no seventh has none */
    HarmonyVoiceResolution triad = move(CH(5), i, 3, 3, true);
    assert(!harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), &triad));
}

static void test_seventh_of_other_chords_and_inversions(void) {
    /* ii7 (re, fa, la, do): the seventh is do (step 7), which goes down to ti (step 6) */
    HarmonyVoiceResolution ii7 = move(CH7(2), CH(5), 7, 6, false);
    assert(harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), &ii7) &&
           harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &ii7));
    /* V4/3 (an inversion): fa still goes down */
    HarmonyVoiceResolution v43 = move(CH7I(5, 2), CH(1), 3, 3, false);
    assert(harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), &v43) &&
           !harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &v43));
}

static void test_leading_tone_in_an_outer_voice(void) {
    HarmonyChord v = CH(5);
    HarmonyChord i = CH(1);
    HarmonyVoiceResolution rising = move(v, i, 6, 7, true); /* ti to do in the soprano */
    assert(harmony_resolution_rule_applies(R(LEADING_TONE_RESOLVES), &rising));
    assert(harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &rising));
    HarmonyVoiceResolution falling = move(v, i, 6, 5, true); /* ti falls in the soprano */
    assert(!harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &falling));
    HarmonyVoiceResolution to_sol = move(CH7(5), i, 6, 4, true); /* the drop is for inner voices only */
    to_sol.complete_dominant_seventh = true;
    assert(!harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &to_sol));
}

static void test_leading_tone_in_an_inner_voice(void) {
    HarmonyChord i = CH(1);
    HarmonyVoiceResolution drop = move(CH7(5), i, 6, 4, false); /* ti down to sol in a complete V7 */
    drop.complete_dominant_seventh = true;
    assert(harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &drop));
    drop.complete_dominant_seventh = false; /* an incomplete V7, or a triad: no drop */
    assert(!harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &drop));
    HarmonyVoiceResolution triad_drop = move(CH(5), i, 6, 4, false);
    triad_drop.complete_dominant_seventh = true;
    assert(!harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &triad_drop));
    HarmonyVoiceResolution rising = move(CH7(5), i, 6, 7, false);
    assert(harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &rising));
    HarmonyVoiceResolution to_mi = move(CH7(5), i, 6, 2, false); /* never to mi */
    to_mi.complete_dominant_seventh = true;
    assert(!harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &to_mi));
}

static void test_leading_tone_applicability(void) {
    HarmonyChord i = CH(1);
    /* the vii-diminished has the leading tone, and the next chord must be a tonic */
    HarmonyVoiceResolution vii = move(VII(1), i, 6, 7, true);
    assert(harmony_resolution_rule_applies(R(LEADING_TONE_RESOLVES), &vii));
    HarmonyVoiceResolution to_vi = move(CH(5), CH(6), 6, 5, true); /* V to vi: not a tonic, the rule does not apply */
    assert(!harmony_resolution_rule_applies(R(LEADING_TONE_RESOLVES), &to_vi) &&
           harmony_resolution_rule_holds(R(LEADING_TONE_RESOLVES), &to_vi));
    /* the voice is not on ti */
    HarmonyVoiceResolution other = move(CH(5), i, 4, 4, true);
    assert(!harmony_resolution_rule_applies(R(LEADING_TONE_RESOLVES), &other));
    /* minor: a VII (the subtonic) has no leading tone, a vii-diminished on the raised seventh does */
    HarmonyVoiceResolution subtonic = move(CH(7), i, 6, 5, true);
    subtonic.mode = HARMONY_MODE_MINOR;
    assert(!harmony_resolution_rule_applies(R(LEADING_TONE_RESOLVES), &subtonic));
    HarmonyVoiceResolution raised = move(VIIM(0), i, 6, 7, true);
    raised.mode = HARMONY_MODE_MINOR;
    assert(harmony_resolution_rule_applies(R(LEADING_TONE_RESOLVES), &raised));
}

static void test_invalid_subjects(void) {
    HarmonyVoiceResolution bad = move(CH(9), CH(1), 3, 2, true);
    assert(!harmony_resolution_valid(&bad));
    assert(!harmony_resolution_rule_holds(R(SEVENTH_RESOLVES_DOWN), &bad));
    assert(!harmony_resolution_rule_applies(R(SEVENTH_RESOLVES_DOWN), NULL));
    HarmonyVoiceResolution ok = move(CH7(5), CH(1), 3, 2, true);
    assert(!harmony_resolution_rule_holds(R(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_seventh_resolves_down_by_step();
    test_seventh_of_other_chords_and_inversions();
    test_leading_tone_in_an_outer_voice();
    test_leading_tone_in_an_inner_voice();
    test_leading_tone_applicability();
    test_invalid_subjects();
    printf("standard-harmony resolution rules: all tests passed\n");
    return 0;
}
