#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_rule_check.h"
#include "harmony_seventh_rules.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "seventh", HARMONY_SEVENTH_RULE_COUNT);
#define harmony_seventh_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_seventh_constraint, harmony_seventh_rule_holds, HarmonySeventhResolution, which, subject)

#define SEVENTH(name) HARMONY_SEVENTH_##name

#define SOL 5
#define TI 7
#define RE 2
#define FA 4
#define DO 1
#define MI 3

/* Degrees from the soprano to the bass for V7 and then I; V7 has the leading tone. */
static HarmonySeventhResolution resolution(HarmonyMode mode, int d0, int d1, int d2, int d3, int t0, int t1, int t2,
                                           int t3) {
    return (HarmonySeventhResolution){mode, {d0, d1, d2, d3}, {t0, t1, t2, t3}, true};
}

static bool holds(HarmonySeventhRule rule, const HarmonySeventhResolution *r) {
    return harmony_seventh_rule_holds(rule, r);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_seventh_rule, harmony_seventh_constraint, HARMONY_SEVENTH_RULE_COUNT);
    for (int i = 0; i < HARMONY_SEVENTH_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_seventh_constraint((HarmonySeventhRule)i));
        assert(type == (i <= SEVENTH(MINOR_LEADING_TONE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(harmony_seventh_rule_name(SEVENTH(BASS_SOL_TO_DO)), "bass-sol-to-do") == 0);
    assert(strcmp(harmony_seventh_rule_name(SEVENTH(V7_NOTES)), "v7-notes") == 0);
    assert(strcmp(harmony_seventh_rule_name(SEVENTH(ACTIVE_NOTES_RESOLVE)), "active-notes-resolve") == 0);
    assert(strcmp(harmony_seventh_rule_name(SEVENTH(TONIC_NOTES)), "tonic-notes") == 0);
    assert(strcmp(harmony_seventh_rule_name(SEVENTH(MINOR_LEADING_TONE)), "minor-leading-tone") == 0);
    assert(strcmp(harmony_seventh_rule_name(SEVENTH(AVOID_RE_TO_MI)), "avoid-re-to-mi") == 0);
    assert(harmony_seventh_rule_name(SEVENTH(RULE_COUNT)) == NULL);
}

static bool all_hold(const HarmonySeventhResolution *r) {
    for (int i = 0; i < HARMONY_SEVENTH_RULE_COUNT; i++) {
        if (!holds((HarmonySeventhRule)i, r)) {
            return false;
        }
    }
    return true;
}

static void test_default_resolutions(void) {
    /* An active note in the soprano, resolved by its tendency, in major and
       minor: the tonic chord has three roots and one third. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonySeventhResolution good[] = {
            resolution(m, FA, TI, RE, SOL, MI, DO, DO, DO), /* fa to mi in the soprano */
            resolution(m, RE, FA, TI, SOL, DO, MI, DO, DO), /* re to do */
            resolution(m, TI, RE, FA, SOL, DO, DO, MI, DO), /* ti to do */
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(harmony_seventh_valid(&good[i]));
            assert(all_hold(&good[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_seventh_check(&good[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_bass(void) {
    HarmonySeventhResolution stays = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, DO, SOL);
    HarmonySeventhResolution not_sol = resolution(HARMONY_MODE_MAJOR, FA, TI, SOL, RE, MI, DO, DO, DO);
    assert(!holds(SEVENTH(BASS_SOL_TO_DO), &stays));
    assert(!holds(SEVENTH(BASS_SOL_TO_DO), &not_sol));
}

static void test_v7_notes(void) {
    /* Missing the seventh, the leading tone, or the fifth with no doubled root. */
    HarmonySeventhResolution no_fa = resolution(HARMONY_MODE_MAJOR, SOL, TI, RE, SOL, DO, DO, DO, DO);
    HarmonySeventhResolution no_ti = resolution(HARMONY_MODE_MAJOR, FA, RE, RE, SOL, MI, DO, DO, DO);
    HarmonySeventhResolution foreign = resolution(HARMONY_MODE_MAJOR, FA, TI, 6, SOL, MI, DO, DO, DO);
    assert(!holds(SEVENTH(V7_NOTES), &no_fa));
    assert(!holds(SEVENTH(V7_NOTES), &no_ti));
    assert(!holds(SEVENTH(V7_NOTES), &foreign));
}

static void test_active_notes_resolve(void) {
    /* The seventh up, the leading tone leaping to mi, re leaping to sol. */
    HarmonySeventhResolution fa_up = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, SOL, DO, DO, DO);
    HarmonySeventhResolution fa_stays = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, FA, DO, DO, DO);
    HarmonySeventhResolution ti_to_mi = resolution(HARMONY_MODE_MAJOR, TI, FA, RE, SOL, MI, MI, DO, DO);
    HarmonySeventhResolution re_to_sol = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, SOL, DO);
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &fa_up));
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &fa_stays));
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &ti_to_mi));
    assert(!holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &re_to_sol));
}

static void test_tonic_notes(void) {
    HarmonySeventhResolution no_third = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, DO, DO, DO, DO);
    HarmonySeventhResolution foreign_note = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, 6, DO);
    HarmonySeventhResolution no_root = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, MI, MI, SOL);
    assert(!holds(SEVENTH(TONIC_NOTES), &no_third));
    assert(!holds(SEVENTH(TONIC_NOTES), &foreign_note));
    assert(!holds(SEVENTH(TONIC_NOTES), &no_root));
}

static void test_minor_needs_the_leading_tone(void) {
    HarmonySeventhResolution minor = resolution(HARMONY_MODE_MINOR, FA, TI, RE, SOL, MI, DO, DO, DO);
    minor.dominant_has_leading_tone = false; /* te instead of ti: a minor seventh chord */
    assert(!holds(SEVENTH(MINOR_LEADING_TONE), &minor));
    minor.dominant_has_leading_tone = true;
    assert(holds(SEVENTH(MINOR_LEADING_TONE), &minor));

    HarmonySeventhResolution major = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, DO, DO);
    major.dominant_has_leading_tone = false;
    assert(holds(SEVENTH(MINOR_LEADING_TONE), &major));
}

static void test_avoid_re_to_mi_is_soft(void) {
    /* re up to mi is a possible resolution, but it makes two roots and two
       thirds, which the chapter suggests avoiding. */
    HarmonySeventhResolution r = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, MI, DO);
    assert(holds(SEVENTH(ACTIVE_NOTES_RESOLVE), &r));
    assert(!holds(SEVENTH(AVOID_RE_TO_MI), &r));
    unsigned hard = 99, soft = 99;
    assert(harmony_seventh_check(&r, &hard, &soft));
    assert(hard == 0 && soft == (1u << SEVENTH(AVOID_RE_TO_MI)));
}

static void test_check_reports_each_failure(void) {
    HarmonySeventhResolution bad = resolution(HARMONY_MODE_MINOR, FA, TI, RE, SOL, FA, DO, DO, SOL);
    bad.dominant_has_leading_tone = false;
    unsigned hard = 0, soft = 99;
    assert(harmony_seventh_check(&bad, &hard, &soft));
    assert(hard == ((1u << SEVENTH(BASS_SOL_TO_DO)) | (1u << SEVENTH(ACTIVE_NOTES_RESOLVE)) |
                    (1u << SEVENTH(TONIC_NOTES)) | (1u << SEVENTH(MINOR_LEADING_TONE))));
    assert(soft == 0);
}

static void test_invalid_resolution(void) {
    HarmonySeventhResolution bad_mode = resolution(HARMONY_MODE_COUNT, FA, TI, RE, SOL, MI, DO, DO, DO);
    HarmonySeventhResolution bad_degree = resolution(HARMONY_MODE_MAJOR, 0, TI, RE, SOL, MI, DO, DO, DO);
    HarmonySeventhResolution bad_tonic = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, DO, 8);
    HarmonySeventhResolution *bad[] = {&bad_mode, &bad_degree, &bad_tonic};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_seventh_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_SEVENTH_RULE_COUNT; rule++) {
            assert(!holds((HarmonySeventhRule)rule, bad[i]));
        }
        assert(!harmony_seventh_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_seventh_valid(NULL));
    HarmonySeventhResolution ok = resolution(HARMONY_MODE_MAJOR, FA, TI, RE, SOL, MI, DO, DO, DO);
    assert(!holds(SEVENTH(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_default_resolutions();
    test_bass();
    test_v7_notes();
    test_active_notes_resolve();
    test_tonic_notes();
    test_minor_needs_the_leading_tone();
    test_avoid_re_to_mi_is_soft();
    test_check_reports_each_failure();
    test_invalid_resolution();
    printf("test_harmony_seventh_rules: ok\n");
    return 0;
}
