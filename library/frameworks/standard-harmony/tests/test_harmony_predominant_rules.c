#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_predominant_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"

#define PD(name) HARMONY_PREDOMINANT_##name

/* Notes are diatonic steps from the soprano to the bass (do is 0, re is 1, ti is
   6, ti below do is -1, a step of -7 is an octave lower). `tonic` is NULL for a
   half cadence. V has the leading tone. */
static HarmonyPredominantEnding ending(HarmonyMode mode, HarmonyChord predominant, const int predominant_steps[4],
                                       const int dominant_steps[4], const int *tonic_steps) {
    HarmonyPredominantEnding e;
    memset(&e, 0, sizeof(e));
    e.mode = mode;
    e.predominant = predominant;
    memcpy(e.predominant_steps, predominant_steps, sizeof(e.predominant_steps));
    memcpy(e.dominant_steps, dominant_steps, sizeof(e.dominant_steps));
    if (tonic_steps != NULL) {
        memcpy(e.tonic_steps, tonic_steps, sizeof(e.tonic_steps));
        e.has_tonic = true;
    }
    e.dominant_has_leading_tone = true;
    return e;
}

static bool holds(HarmonyPredominantRule rule, const HarmonyPredominantEnding *e) {
    return harmony_predominant_rule_holds(rule, e);
}

static bool all_hold(const HarmonyPredominantEnding *e) {
    for (int i = 0; i < HARMONY_PREDOMINANT_RULE_COUNT; i++) {
        if (!holds((HarmonyPredominantRule)i, e)) {
            return false;
        }
    }
    return true;
}

/* Authentic cadences: fa to sol to do in the bass, ti in the soprano over V
   resolving to do over I, the approach from above or by common tone, with no
   parallels. */
static const int II6_PD[4] = {1, -6, -11, -11};
static const int IV_PD[4] = {0, -4, -9, -11};
static const int II_PD[4] = {1, -6, -11, -13};
static const int AC_V[4] = {-1, -3, -10, -17};
static const int AC_I[4] = {0, -5, -10, -14};
/* Half cadences: the same without I. */
static const int II6_HC_V[4] = {-1, -6, -10, -17};
static const int IV_HC_V[4] = {-1, -6, -8, -17};
static const int II_HC_V[4] = {-1, -6, -10, -17};

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_predominant_rule, harmony_predominant_constraint,
                                    HARMONY_PREDOMINANT_RULE_COUNT);
    for (int i = 0; i < HARMONY_PREDOMINANT_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_predominant_constraint((HarmonyPredominantRule)i));
        assert(type == (i <= PD(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PREDOMINANT_RULE_COUNT] = {
        "strong-predominant", "bass-motion",         "chord-notes",
        "active-note-resolves", "minor-leading-tone", "no-parallels",
        "not-all-voices-same-direction", "soprano-approach", "iv-doubles-bass",
        "ii-soprano",         "root-ii-major-only"};
    for (int i = 0; i < HARMONY_PREDOMINANT_RULE_COUNT; i++) {
        assert(strcmp(harmony_predominant_rule_name((HarmonyPredominantRule)i), expected[i]) == 0);
    }
    assert(harmony_predominant_rule_name(PD(RULE_COUNT)) == NULL);
}

static void test_authentic_cadences(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyPredominantEnding good[] = {
            ending(m, CHI(2, 1), II6_PD, AC_V, AC_I),
            ending(m, CH(4), IV_PD, AC_V, AC_I),
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(harmony_predominant_ending_valid(&good[i]));
            assert(all_hold(&good[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_predominant_check(&good[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
    /* Root position ii: a major key only, with re or fa in the soprano. */
    HarmonyPredominantEnding ii = ending(HARMONY_MODE_MAJOR, CH(2), II_PD, AC_V, AC_I);
    assert(all_hold(&ii));
}

static void test_half_cadences(void) {
    /* Leave off the I chord: the rules about it have nothing to check. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyPredominantEnding good[] = {
            ending(m, CHI(2, 1), II6_PD, II6_HC_V, NULL),
            ending(m, CH(4), IV_PD, IV_HC_V, NULL),
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(!good[i].has_tonic);
            assert(all_hold(&good[i]));
        }
    }
    HarmonyPredominantEnding ii = ending(HARMONY_MODE_MAJOR, CH(2), II_PD, II_HC_V, NULL);
    assert(all_hold(&ii));

    /* Whatever sits in the unused tonic steps is ignored. */
    ii.tonic_steps[0] = 99;
    ii.tonic_steps[3] = -99;
    assert(all_hold(&ii));
}

static void test_strong_predominant(void) {
    /* vi, V, a seventh chord, and other inversions are not strong predominants. */
    HarmonyChord chords[] = {CH(6), CH(5), CH(1), CH7(2), CHI(4, 1), CHI(2, 2)};
    for (size_t i = 0; i < COUNT_OF(chords); i++) {
        HarmonyPredominantEnding e = ending(HARMONY_MODE_MAJOR, chords[i], II6_PD, AC_V, AC_I);
        assert(!holds(PD(STRONG_PREDOMINANT), &e));
    }
    /* The bass and the notes cannot be checked for one, so those fail too. */
    HarmonyPredominantEnding vi = ending(HARMONY_MODE_MAJOR, CH(6), II6_PD, AC_V, AC_I);
    unsigned hard = 0, soft = 99;
    assert(harmony_predominant_check(&vi, &hard, &soft));
    assert(hard == ((1u << PD(STRONG_PREDOMINANT)) | (1u << PD(BASS_MOTION)) | (1u << PD(CHORD_NOTES))));
    assert(soft == 0);
}

static void test_bass_motion(void) {
    /* The bass has to be fa for IV and ii6 (re for ii), sol over V, and do over I. */
    const int ii6_bass_re[4] = {1, -6, -11, -13};
    const int v_bass_not_sol[4] = {-1, -3, -10, -16};
    const int i_bass_not_do[4] = {0, -5, -10, -13};
    HarmonyPredominantEnding wrong_pd_bass = ending(HARMONY_MODE_MAJOR, CHI(2, 1), ii6_bass_re, AC_V, AC_I);
    HarmonyPredominantEnding wrong_v_bass = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, v_bass_not_sol, AC_I);
    HarmonyPredominantEnding wrong_i_bass = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, AC_V, i_bass_not_do);
    assert(!holds(PD(BASS_MOTION), &wrong_pd_bass));
    assert(!holds(PD(BASS_MOTION), &wrong_v_bass));
    assert(!holds(PD(BASS_MOTION), &wrong_i_bass));
    /* Root position ii has re in the bass, not fa. */
    HarmonyPredominantEnding ii_with_fa = ending(HARMONY_MODE_MAJOR, CH(2), II6_PD, AC_V, AC_I);
    assert(!holds(PD(BASS_MOTION), &ii_with_fa));
    /* Without a tonic, the I bass is not checked. */
    HarmonyPredominantEnding hc = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, II6_HC_V, NULL);
    assert(holds(PD(BASS_MOTION), &hc));
}

static void test_chord_notes(void) {
    /* A foreign note in the IV, a V with no leading tone, an I with no third. */
    const int iv_with_re[4] = {1, -4, -9, -11};
    const int v_without_ti[4] = {-3, -3, -10, -17};
    const int i_without_mi[4] = {0, -7, -10, -14};
    const int i_with_re[4] = {0, -6, -10, -14};
    HarmonyPredominantEnding foreign_note = ending(HARMONY_MODE_MAJOR, CH(4), iv_with_re, AC_V, AC_I);
    HarmonyPredominantEnding no_leading_tone = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, v_without_ti, AC_I);
    HarmonyPredominantEnding no_third = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, AC_V, i_without_mi);
    HarmonyPredominantEnding foreign_tonic = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, AC_V, i_with_re);
    assert(!holds(PD(CHORD_NOTES), &foreign_note));
    assert(!holds(PD(CHORD_NOTES), &no_leading_tone));
    assert(!holds(PD(CHORD_NOTES), &no_third));
    assert(!holds(PD(CHORD_NOTES), &foreign_tonic));
    /* ii and ii6 take re, fa, and la; IV does not take re. */
    HarmonyPredominantEnding ii6_with_do = ending(HARMONY_MODE_MAJOR, CHI(2, 1), IV_PD, AC_V, AC_I);
    assert(!holds(PD(CHORD_NOTES), &ii6_with_do));
}

static void test_active_notes_resolve(void) {
    /* ti up to do, re down to do; anywhere else is wrong. */
    const int v_ti_soprano[4] = {-1, -3, -10, -17};
    const int v_re_soprano[4] = {1, -3, -10, -17};
    const int ti_to_do[4] = {0, -5, -10, -14};
    const int ti_to_mi[4] = {-5, -5, -10, -14};
    const int re_to_do[4] = {0, -5, -10, -14};
    const int re_to_mi[4] = {2, -5, -10, -14};
    HarmonyPredominantEnding ok_ti = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, v_ti_soprano, ti_to_do);
    HarmonyPredominantEnding bad_ti = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, v_ti_soprano, ti_to_mi);
    HarmonyPredominantEnding ok_re = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, v_re_soprano, re_to_do);
    HarmonyPredominantEnding bad_re = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, v_re_soprano, re_to_mi);
    assert(holds(PD(ACTIVE_NOTE_RESOLVES), &ok_ti));
    assert(!holds(PD(ACTIVE_NOTE_RESOLVES), &bad_ti));
    assert(holds(PD(ACTIVE_NOTE_RESOLVES), &ok_re));
    assert(!holds(PD(ACTIVE_NOTE_RESOLVES), &bad_re));
    /* ti in an octave other than the one that steps to do: ti to the do an octave
       down leaps, which is not the tendency. */
    const int ti_to_low_do[4] = {-7, -5, -10, -14};
    HarmonyPredominantEnding leap = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, v_ti_soprano, ti_to_low_do);
    assert(!holds(PD(ACTIVE_NOTE_RESOLVES), &leap));
    /* A half cadence has no resolution to check. */
    HarmonyPredominantEnding hc = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, IV_HC_V, NULL);
    assert(holds(PD(ACTIVE_NOTE_RESOLVES), &hc));
}

static void test_minor_needs_the_leading_tone(void) {
    HarmonyPredominantEnding minor = ending(HARMONY_MODE_MINOR, CHI(2, 1), II6_PD, AC_V, AC_I);
    minor.dominant_has_leading_tone = false;
    assert(!holds(PD(MINOR_LEADING_TONE), &minor));
    minor.dominant_has_leading_tone = true;
    assert(holds(PD(MINOR_LEADING_TONE), &minor));
    HarmonyPredominantEnding major = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, AC_V, AC_I);
    major.dominant_has_leading_tone = false;
    assert(holds(PD(MINOR_LEADING_TONE), &major));
}

static void test_parallels_into_v(void) {
    /* Tenor and bass both go up a step from an octave: parallel octaves. */
    const int v_parallel[4] = {-1, -3, -10, -10};
    HarmonyPredominantEnding octaves = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, v_parallel, NULL);
    assert(!holds(PD(NO_PARALLELS), &octaves));
    /* IV moves up by step to V: with the bass and a tenor a fifth apart, both
       moving up a step makes parallel fifths. */
    const int iv_fifth[4] = {7, 3, 0, -4};
    const int v_fifth[4] = {6, 3, 1, -3};
    HarmonyPredominantEnding fifths = ending(HARMONY_MODE_MAJOR, CH(4), iv_fifth, v_fifth, NULL);
    assert(!holds(PD(NO_PARALLELS), &fifths));
    /* Contrary motion with the bass avoids it. */
    HarmonyPredominantEnding fine = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, II6_HC_V, NULL);
    assert(holds(PD(NO_PARALLELS), &fine));
}

static void test_parallels_into_i(void) {
    /* V to I: soprano and alto a fifth apart, both going down a step. */
    const int v[4] = {1, -3, -10, -17};
    const int i_parallel[4] = {0, -4, -10, -14};
    HarmonyPredominantEnding fifths = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, v, i_parallel);
    assert(!holds(PD(NO_PARALLELS), &fifths));
    /* Without a tonic, V to I is not looked at. */
    HarmonyPredominantEnding hc = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, v, NULL);
    assert(holds(PD(NO_PARALLELS), &hc));
}

static void test_diminished_fifth_is_not_a_parallel_fifth(void) {
    /* A diminished fifth (ti with fa above it) moving to a perfect fifth, both
       voices going down, is not parallel fifths. */
    const int pd[4] = {3, -1, -11, -11};
    const int v[4] = {2, -2, -10, -17};
    HarmonyPredominantEnding e = ending(HARMONY_MODE_MAJOR, CHI(2, 1), pd, v, NULL);
    assert(holds(PD(NO_PARALLELS), &e));
}

static void test_not_all_voices_same_direction(void) {
    const int all_up[4] = {1, -3, -8, -10};
    HarmonyPredominantEnding up = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, all_up, NULL);
    assert(!holds(PD(NOT_ALL_VOICES_SAME_DIRECTION), &up));
    const int all_down[4] = {-1, -5, -10, -12};
    HarmonyPredominantEnding down = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, all_down, NULL);
    assert(!holds(PD(NOT_ALL_VOICES_SAME_DIRECTION), &down));
    /* One voice staying, or moving the other way, is enough. */
    const int one_stays[4] = {0, -3, -8, -10};
    HarmonyPredominantEnding stays = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, one_stays, NULL);
    assert(holds(PD(NOT_ALL_VOICES_SAME_DIRECTION), &stays));
    HarmonyPredominantEnding contrary = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, IV_HC_V, NULL);
    assert(holds(PD(NOT_ALL_VOICES_SAME_DIRECTION), &contrary));
}

static void test_soprano_approach(void) {
    /* From above, and by common tone, are fine; from below is not. */
    const int from_above[4] = {1, -6, -11, -11};
    const int common_tone[4] = {1, -6, -11, -11};
    const int dominant_with_re[4] = {1, -3, -10, -17};
    HarmonyPredominantEnding above = ending(HARMONY_MODE_MAJOR, CHI(2, 1), from_above, AC_V, NULL);
    HarmonyPredominantEnding common = ending(HARMONY_MODE_MAJOR, CHI(2, 1), common_tone, dominant_with_re, NULL);
    HarmonyPredominantEnding below = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, dominant_with_re, NULL);
    assert(holds(PD(SOPRANO_APPROACH), &above));
    assert(holds(PD(SOPRANO_APPROACH), &common));
    assert(!holds(PD(SOPRANO_APPROACH), &below));
}

static void test_iv_doubles_its_bass(void) {
    /* fa once in a IV is a soft failure; ii6 may double anything. */
    const int single_fa[4] = {7, 0, -2, -11};
    HarmonyPredominantEnding single = ending(HARMONY_MODE_MAJOR, CH(4), single_fa, AC_V, NULL);
    assert(!holds(PD(IV_DOUBLES_BASS), &single));
    HarmonyPredominantEnding doubled = ending(HARMONY_MODE_MAJOR, CH(4), IV_PD, AC_V, NULL);
    assert(holds(PD(IV_DOUBLES_BASS), &doubled));
    HarmonyPredominantEnding ii6 = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, AC_V, NULL);
    assert(holds(PD(IV_DOUBLES_BASS), &ii6));
}

static void test_root_position_ii_soprano(void) {
    /* re or fa over ii, not la. */
    const int soprano_re[4] = {1, -6, -11, -13};
    const int soprano_fa[4] = {3, -6, -11, -13};
    const int soprano_la[4] = {5, -6, -11, -13};
    HarmonyPredominantEnding re = ending(HARMONY_MODE_MAJOR, CH(2), soprano_re, AC_V, NULL);
    HarmonyPredominantEnding fa = ending(HARMONY_MODE_MAJOR, CH(2), soprano_fa, AC_V, NULL);
    HarmonyPredominantEnding la = ending(HARMONY_MODE_MAJOR, CH(2), soprano_la, AC_V, NULL);
    assert(holds(PD(II_SOPRANO), &re));
    assert(holds(PD(II_SOPRANO), &fa));
    assert(!holds(PD(II_SOPRANO), &la));
    /* The rule is about ii only: la over ii6 is fine. */
    HarmonyPredominantEnding ii6_la = ending(HARMONY_MODE_MAJOR, CHI(2, 1), soprano_la, AC_V, NULL);
    assert(holds(PD(II_SOPRANO), &ii6_la));
}

static void test_root_position_ii_in_minor(void) {
    HarmonyPredominantEnding minor = ending(HARMONY_MODE_MINOR, CH(2), II_PD, AC_V, AC_I);
    assert(!holds(PD(ROOT_II_MAJOR_ONLY), &minor));
    /* It is advice only: nothing hard breaks. */
    unsigned hard = 99, soft = 99;
    assert(harmony_predominant_check(&minor, &hard, &soft));
    assert(hard == 0 && soft == (1u << PD(ROOT_II_MAJOR_ONLY)));
    /* ii6 and IV are for either mode. */
    HarmonyPredominantEnding ii6 = ending(HARMONY_MODE_MINOR, CHI(2, 1), II6_PD, AC_V, AC_I);
    assert(holds(PD(ROOT_II_MAJOR_ONLY), &ii6));
}

static void test_invalid_ending(void) {
    HarmonyPredominantEnding bad_mode = ending(HARMONY_MODE_COUNT, CHI(2, 1), II6_PD, AC_V, AC_I);
    HarmonyPredominantEnding bad_chord = ending(HARMONY_MODE_MAJOR, CH(9), II6_PD, AC_V, AC_I);
    HarmonyPredominantEnding *bad[] = {&bad_mode, &bad_chord};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_predominant_ending_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_PREDOMINANT_RULE_COUNT; rule++) {
            assert(!holds((HarmonyPredominantRule)rule, bad[i]));
        }
        assert(!harmony_predominant_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_predominant_ending_valid(NULL));
    HarmonyPredominantEnding ok = ending(HARMONY_MODE_MAJOR, CHI(2, 1), II6_PD, AC_V, AC_I);
    assert(!holds(PD(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_authentic_cadences();
    test_half_cadences();
    test_strong_predominant();
    test_bass_motion();
    test_chord_notes();
    test_active_notes_resolve();
    test_minor_needs_the_leading_tone();
    test_parallels_into_v();
    test_parallels_into_i();
    test_diminished_fifth_is_not_a_parallel_fifth();
    test_not_all_voices_same_direction();
    test_soprano_approach();
    test_iv_doubles_its_bass();
    test_root_position_ii_soprano();
    test_root_position_ii_in_minor();
    test_invalid_ending();
    printf("test_harmony_predominant_rules: ok\n");
    return 0;
}
