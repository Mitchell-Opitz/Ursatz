#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_cadential_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "cadential", HARMONY_CADENTIAL_RULE_COUNT);
#define harmony_cadential_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_cadential_constraint, harmony_cadential_rule_holds, HarmonyCadentialSixFour, which, subject)

#define CAD(name) HARMONY_CADENTIAL_##name

/* Notes are diatonic steps from the soprano to the bass (do is 0, re is 1, ti
   below do is -1, an octave is 7). V has the leading tone. There is no approach
   chord unless a test adds one. */
static HarmonyCadentialSixFour cadential(HarmonyMode mode, const int six_four[4], const int dominant[4]) {
    HarmonyCadentialSixFour c;
    memset(&c, 0, sizeof(c));
    c.mode = mode;
    memcpy(c.six_four_steps, six_four, sizeof(c.six_four_steps));
    memcpy(c.dominant_steps, dominant, sizeof(c.dominant_steps));
    c.dominant_has_leading_tone = true;
    c.six_four_beat = BEAT_STRENGTH_STRONG; /* the 6/4 on a stronger beat than the V */
    c.dominant_beat = BEAT_STRENGTH_WEAK;
    return c;
}

static bool holds(HarmonyCadentialRule rule, const HarmonyCadentialSixFour *c) {
    return harmony_cadential_rule_holds(rule, c);
}

static bool all_hold(const HarmonyCadentialSixFour *c) {
    for (int i = 0; i < HARMONY_CADENTIAL_RULE_COUNT; i++) {
        if (!holds((HarmonyCadentialRule)i, c)) {
            return false;
        }
    }
    return true;
}

/* mi in the soprano, do in the alto, the bass doubled in the tenor. */
static const int SIX_FOUR_MI_TOP[4] = {2, 0, -3, -10};
static const int V_TRIAD[4] = {1, -1, -3, -10};  /* 6-5, 4-3, the double stays on sol */
static const int V_SEVENTH[4] = {1, -1, -4, -10}; /* the double falls to fa: 8-7 */
/* do in the soprano, mi in the alto. */
static const int SIX_FOUR_DO_TOP[4] = {7, 2, -3, -10};
static const int V_FROM_DO_TOP[4] = {6, 1, -3, -10};

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_cadential_rule, harmony_cadential_constraint,
                                    HARMONY_CADENTIAL_RULE_COUNT);
    for (int i = 0; i < HARMONY_CADENTIAL_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_cadential_constraint((HarmonyCadentialRule)i));
        assert(type == (i <= CAD(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_CADENTIAL_RULE_COUNT] = {
        "bass-stays",     "six-four-notes",    "resolves-six-five-four-three",
        "doubling-voice", "minor-leading-tone", "no-parallels",
        "smooth-approach", "no-leap-to-fourth", "approach-strong-predominant", "six-four-stronger-beat"};
    for (int i = 0; i < HARMONY_CADENTIAL_RULE_COUNT; i++) {
        assert(strcmp(harmony_cadential_rule_name((HarmonyCadentialRule)i), expected[i]) == 0);
    }
    assert(harmony_cadential_rule_name(CAD(RULE_COUNT)) == NULL);
}

static void test_resolution_to_v_and_v7(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyCadentialSixFour good[] = {
            cadential(m, SIX_FOUR_MI_TOP, V_TRIAD),
            cadential(m, SIX_FOUR_MI_TOP, V_SEVENTH),
            cadential(m, SIX_FOUR_DO_TOP, V_FROM_DO_TOP),
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(harmony_cadential_valid(&good[i]));
            assert(all_hold(&good[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_cadential_check(&good[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_resolves_to_seventh(void) {
    bool seventh = true;
    HarmonyCadentialSixFour triad = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    HarmonyCadentialSixFour with_seventh = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_SEVENTH);
    assert(harmony_cadential_resolves_to_seventh(&triad, &seventh));
    assert(!seventh);
    assert(harmony_cadential_resolves_to_seventh(&with_seventh, &seventh));
    assert(seventh);

    /* A cadential 6/4 that breaks a hard rule resolves to neither. */
    seventh = true;
    const int broken[4] = {2, -1, -3, -10};
    HarmonyCadentialSixFour bad = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, broken);
    assert(!harmony_cadential_resolves_to_seventh(&bad, &seventh));
    assert(seventh);
    assert(!harmony_cadential_resolves_to_seventh(&triad, NULL));
    assert(!harmony_cadential_resolves_to_seventh(NULL, &seventh));
}

static void test_bass_stays_on_sol(void) {
    const int bass_moves_down[4] = {1, -1, -3, -11};
    const int bass_moves_up[4] = {1, -1, -3, -9};
    const int bass_octave_lower[4] = {1, -1, -3, -17};
    HarmonyCadentialSixFour moves_down = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, bass_moves_down);
    HarmonyCadentialSixFour moves_up = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, bass_moves_up);
    HarmonyCadentialSixFour leaps = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, bass_octave_lower);
    assert(!holds(CAD(BASS_STAYS), &moves_down));
    assert(!holds(CAD(BASS_STAYS), &moves_up));
    assert(!holds(CAD(BASS_STAYS), &leaps));
    /* The bass is sol in the 6/4 itself. */
    const int bass_not_sol[4] = {2, 0, -3, -9};
    HarmonyCadentialSixFour not_sol = cadential(HARMONY_MODE_MAJOR, bass_not_sol, bass_not_sol);
    assert(!holds(CAD(BASS_STAYS), &not_sol));
}

static void test_six_four_notes(void) {
    /* The bass has to be doubled, and the sixth and the fourth each sound once. */
    const int no_double[4] = {2, 0, -1, -10};     /* mi, do, ti, sol */
    const int two_sixths[4] = {2, 2, -3, -10};    /* mi, mi, sol, sol: no fourth */
    const int three_sols[4] = {0, -3, -3, -10};   /* do, sol, sol, sol: no sixth */
    const int two_fourths[4] = {7, 0, -3, -10};   /* do, do, sol, sol: no sixth */
    const int extra_note[4] = {2, 0, -5, -10};    /* mi, do, mi, sol */
    const int *bad[] = {no_double, two_sixths, three_sols, two_fourths, extra_note};
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, bad[i], V_TRIAD);
        assert(!holds(CAD(SIX_FOUR_NOTES), &c));
    }
}

static void test_six_five_four_three(void) {
    /* Each voice's motion is kept: the sixth falls to the fifth, the fourth to the
       third. */
    const int sixth_stays[4] = {2, -1, -3, -10};   /* mi stays */
    const int sixth_rises[4] = {3, -1, -3, -10};   /* mi up to fa */
    const int fourth_stays[4] = {1, 0, -3, -10};   /* do stays */
    const int fourth_to_re[4] = {1, 1, -3, -10};   /* do up to re */
    const int fourth_leaps[4] = {1, -4, -3, -10};  /* do down to fa: a leap */
    const int sixth_leaps[4] = {-1, -1, -3, -10};  /* mi down to ti: a leap */
    const int *bad[] = {sixth_stays, sixth_rises, fourth_stays, fourth_to_re, fourth_leaps, sixth_leaps};
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, bad[i]);
        assert(!holds(CAD(RESOLVES_SIX_FIVE_FOUR_THREE), &c));
    }
    /* The figures follow the voices, wherever they sit. */
    HarmonyCadentialSixFour swapped = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_DO_TOP, V_FROM_DO_TOP);
    assert(holds(CAD(RESOLVES_SIX_FIVE_FOUR_THREE), &swapped));
}

static void test_doubling_voice(void) {
    /* It stays on sol, or falls a step to fa; nothing else. */
    const int stays[4] = {1, -1, -3, -10};
    const int to_fa[4] = {1, -1, -4, -10};
    const int up_to_la[4] = {1, -1, -2, -10};
    const int down_two[4] = {1, -1, -5, -10};
    const int jumps[4] = {1, -1, 4, -10};
    HarmonyCadentialSixFour stay = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, stays);
    HarmonyCadentialSixFour fa = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, to_fa);
    HarmonyCadentialSixFour la = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, up_to_la);
    HarmonyCadentialSixFour two = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, down_two);
    HarmonyCadentialSixFour jump = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, jumps);
    assert(holds(CAD(DOUBLING_VOICE), &stay));
    assert(holds(CAD(DOUBLING_VOICE), &fa));
    assert(!holds(CAD(DOUBLING_VOICE), &la));
    assert(!holds(CAD(DOUBLING_VOICE), &two));
    assert(!holds(CAD(DOUBLING_VOICE), &jump));
    /* The doubling voice may be any upper voice. */
    const int double_in_alto[4] = {2, -3, 0, -10};
    const int resolved[4] = {1, -4, -1, -10};
    HarmonyCadentialSixFour alto = cadential(HARMONY_MODE_MAJOR, double_in_alto, resolved);
    assert(all_hold(&alto));
}

static void test_minor_needs_the_leading_tone(void) {
    HarmonyCadentialSixFour minor = cadential(HARMONY_MODE_MINOR, SIX_FOUR_MI_TOP, V_TRIAD);
    minor.dominant_has_leading_tone = false;
    assert(!holds(CAD(MINOR_LEADING_TONE), &minor));
    minor.dominant_has_leading_tone = true;
    assert(holds(CAD(MINOR_LEADING_TONE), &minor));
    HarmonyCadentialSixFour major = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    major.dominant_has_leading_tone = false;
    assert(holds(CAD(MINOR_LEADING_TONE), &major));
}

static void test_check_reports_each_failure(void) {
    const int bad_six_four[4] = {2, 0, -1, -9};
    const int bad_dominant[4] = {2, 0, -1, -10};
    HarmonyCadentialSixFour bad = cadential(HARMONY_MODE_MINOR, bad_six_four, bad_dominant);
    bad.dominant_has_leading_tone = false;
    unsigned hard = 0, soft = 99;
    assert(harmony_cadential_check(&bad, &hard, &soft));
    /* bass not sol; no doubled bass; mi and do do not fall; minor without ti */
    assert(hard == ((1u << CAD(BASS_STAYS)) | (1u << CAD(SIX_FOUR_NOTES)) |
                    (1u << CAD(RESOLVES_SIX_FIVE_FOUR_THREE)) | (1u << CAD(MINOR_LEADING_TONE))));
    assert(soft == 0);
}

/* A cadential 6/4 approached by a chord: IV (fa la do) or ii6 (re fa la over fa),
   each voice moving into it by step or common tone, the bass up from fa to sol. */
static const int APPROACH_IV[4] = {3, 0, -2, -11};
static const int APPROACH_II6[4] = {1, 1, -2, -11};

static HarmonyCadentialSixFour approached(HarmonyChord chord, const int steps[4]) {
    HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    c.has_approach = true;
    c.approach = chord;
    memcpy(c.approach_steps, steps, sizeof(c.approach_steps));
    return c;
}

static void test_approach_by_strong_predominants(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyCadentialSixFour iv = approached(CH(4), APPROACH_IV);
        HarmonyCadentialSixFour ii6 = approached(CHI(2, 1), APPROACH_II6);
        iv.mode = ii6.mode = (HarmonyMode)mode;
        assert(harmony_cadential_valid(&iv));
        assert(all_hold(&iv));
        assert(all_hold(&ii6));
        unsigned hard = 99, soft = 99;
        assert(harmony_cadential_check(&iv, &hard, &soft));
        assert(hard == 0 && soft == 0);
    }
}

static void test_no_approach_skips_the_approach_rules(void) {
    HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    /* Whatever is in the approach fields is ignored. */
    c.approach = CH(9);
    c.approach_steps[0] = 99;
    c.approach_steps[3] = -99;
    assert(harmony_cadential_valid(&c));
    assert(holds(CAD(NO_PARALLELS), &c));
    assert(holds(CAD(SMOOTH_APPROACH), &c));
    assert(holds(CAD(NO_LEAP_TO_FOURTH), &c));
    assert(holds(CAD(APPROACH_STRONG_PREDOMINANT), &c));
}

static void test_parallels_into_the_six_four(void) {
    /* The tenor and bass, an octave apart, both move up a step: parallel octaves. */
    const int octaves[4] = {3, 0, -4, -11};
    HarmonyCadentialSixFour c = approached(CH(4), octaves);
    assert(!holds(CAD(NO_PARALLELS), &c));
    /* It is a hard rule, and only that one is broken. */
    unsigned hard = 0, soft = 99;
    assert(harmony_cadential_check(&c, &hard, &soft));
    assert(hard == (1u << CAD(NO_PARALLELS)) && soft == 0);
}

static void test_parallels_out_of_the_six_four(void) {
    /* Two voices an octave apart, both falling a step to the resolution. */
    const int six_four[4] = {7, 0, -3, -10};
    const int resolved[4] = {6, -1, -3, -10};
    HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, six_four, resolved);
    assert(!holds(CAD(NO_PARALLELS), &c));
}

static void test_smooth_approach(void) {
    /* The soprano leaps up a third to the sixth above the bass: rarer. */
    const int to_sixth[4] = {0, 0, -2, -11};
    HarmonyCadentialSixFour sixth = approached(CH(4), to_sixth);
    assert(!holds(CAD(SMOOTH_APPROACH), &sixth));
    assert(holds(CAD(NO_LEAP_TO_FOURTH), &sixth));
    /* The alto leaps down a fourth to the fourth above the bass: much rarer, and
       the one to avoid in particular. */
    const int to_fourth[4] = {3, 3, -2, -11};
    HarmonyCadentialSixFour fourth = approached(CH(4), to_fourth);
    assert(!holds(CAD(SMOOTH_APPROACH), &fourth));
    assert(!holds(CAD(NO_LEAP_TO_FOURTH), &fourth));
    /* The bass leaps into sol in the ordinary way: not counted. */
    const int bass_leaps[4] = {3, 0, -2, -14};
    HarmonyCadentialSixFour bass = approached(CH(4), bass_leaps);
    assert(holds(CAD(SMOOTH_APPROACH), &bass));
    /* Common tones and steps are smooth, up or down. */
    HarmonyCadentialSixFour smooth = approached(CH(4), APPROACH_IV);
    assert(holds(CAD(SMOOTH_APPROACH), &smooth));

    /* Both are advice: soft rules. */
    unsigned hard = 99, soft = 0;
    assert(harmony_cadential_check(&fourth, &hard, &soft));
    assert(hard == 0);
    assert(soft == ((1u << CAD(SMOOTH_APPROACH)) | (1u << CAD(NO_LEAP_TO_FOURTH))));
}

static void test_approach_is_usually_a_strong_predominant(void) {
    HarmonyChord not_strong[] = {CH(6), CH(1), CH(2), CH(5), CH7(4), CHI(4, 1)};
    for (size_t i = 0; i < COUNT_OF(not_strong); i++) {
        HarmonyCadentialSixFour c = approached(not_strong[i], APPROACH_IV);
        assert(!holds(CAD(APPROACH_STRONG_PREDOMINANT), &c));
    }
    /* A weak predominant breaks nothing hard. */
    HarmonyCadentialSixFour vi = approached(CH(6), APPROACH_IV);
    unsigned hard = 99, soft = 0;
    assert(harmony_cadential_check(&vi, &hard, &soft));
    assert(hard == 0 && soft == (1u << CAD(APPROACH_STRONG_PREDOMINANT)));
}

static void test_six_four_is_on_a_stronger_beat(void) {
    const BeatStrength S = BEAT_STRENGTH_STRONG, M = BEAT_STRENGTH_MEDIUM, W = BEAT_STRENGTH_WEAK;
    const BeatStrength good[][2] = {{S, W}, {S, M}, {M, W}};
    const BeatStrength bad[][2] = {{S, S}, {W, W}, {M, M}, {W, S}, {M, S}, {W, M}};
    for (size_t i = 0; i < COUNT_OF(good); i++) {
        HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
        c.six_four_beat = good[i][0];
        c.dominant_beat = good[i][1];
        assert(holds(CAD(SIX_FOUR_STRONGER_BEAT), &c));
    }
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        HarmonyCadentialSixFour c = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
        c.six_four_beat = bad[i][0];
        c.dominant_beat = bad[i][1];
        assert(!holds(CAD(SIX_FOUR_STRONGER_BEAT), &c));
        /* It is advice: nothing hard breaks. */
        unsigned hard = 99, soft = 0;
        assert(harmony_cadential_check(&c, &hard, &soft));
        assert(hard == 0 && soft == (1u << CAD(SIX_FOUR_STRONGER_BEAT)));
    }
    HarmonyCadentialSixFour invalid = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    invalid.dominant_beat = BEAT_STRENGTH_COUNT;
    assert(!harmony_cadential_valid(&invalid));
    assert(!holds(CAD(SIX_FOUR_STRONGER_BEAT), &invalid));
}

static void test_invalid_cadential(void) {
    HarmonyCadentialSixFour bad_mode = cadential(HARMONY_MODE_COUNT, SIX_FOUR_MI_TOP, V_TRIAD);
    HarmonyCadentialSixFour bad_approach = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    bad_approach.has_approach = true;
    bad_approach.approach = CH(9);
    HarmonyCadentialSixFour *bad[] = {&bad_mode, &bad_approach};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_cadential_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_CADENTIAL_RULE_COUNT; rule++) {
            assert(!holds((HarmonyCadentialRule)rule, bad[i]));
        }
        assert(!harmony_cadential_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_cadential_valid(NULL));
    HarmonyCadentialSixFour ok = cadential(HARMONY_MODE_MAJOR, SIX_FOUR_MI_TOP, V_TRIAD);
    assert(!holds(CAD(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_resolution_to_v_and_v7();
    test_resolves_to_seventh();
    test_bass_stays_on_sol();
    test_six_four_notes();
    test_six_five_four_three();
    test_doubling_voice();
    test_minor_needs_the_leading_tone();
    test_check_reports_each_failure();
    test_approach_by_strong_predominants();
    test_no_approach_skips_the_approach_rules();
    test_parallels_into_the_six_four();
    test_parallels_out_of_the_six_four();
    test_smooth_approach();
    test_approach_is_usually_a_strong_predominant();
    test_six_four_is_on_a_stronger_beat();
    test_invalid_cadential();
    printf("test_harmony_cadential_rules: ok\n");
    return 0;
}
