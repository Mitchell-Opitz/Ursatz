#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_writing_rules.h"

#define WRITING(name) HARMONY_WRITING_##name

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define IAC CADENCE_TYPE_IMPERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

/* Bass degrees b0, b1 and soprano degrees s0, s1; V has the leading tone. */
static HarmonyCadenceWriting writing(HarmonyMode mode, CadenceType type, int b0, int b1, int s0, int s1) {
    return (HarmonyCadenceWriting){mode, type, {b0, b1}, {s0, s1}, true};
}

static bool all_hold(const HarmonyCadenceWriting *w) {
    for (int i = 0; i < HARMONY_WRITING_RULE_COUNT; i++) {
        if (!harmony_writing_rule_holds((HarmonyWritingRule)i, w)) {
            return false;
        }
    }
    return true;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_writing_rule, harmony_writing_constraint, HARMONY_WRITING_RULE_COUNT);
    for (int i = 0; i < HARMONY_WRITING_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_writing_constraint((HarmonyWritingRule)i));
        assert(type == (i <= WRITING(MINOR_LEADING_TONE) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(harmony_writing_rule_name(WRITING(BASS_MOTION)), "bass-motion") == 0);
    assert(strcmp(harmony_writing_rule_name(WRITING(SOPRANO_MELODY)), "soprano-melody") == 0);
    assert(strcmp(harmony_writing_rule_name(WRITING(MINOR_LEADING_TONE)), "minor-leading-tone") == 0);
    assert(strcmp(harmony_writing_rule_name(WRITING(COMMON_HC_SOPRANO)), "common-hc-soprano") == 0);
    assert(harmony_writing_rule_name(WRITING(RULE_COUNT)) == NULL);
}

static void test_good_cadences(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyCadenceWriting good[] = {
            writing(m, PAC, 5, 1, 2, 1), writing(m, PAC, 5, 1, 7, 1),
            writing(m, IAC, 5, 1, 2, 3), writing(m, IAC, 5, 1, 5, 5),
            writing(m, HC, 1, 5, 1, 7),  writing(m, HC, 1, 5, 3, 2),
        };
        for (size_t i = 0; i < COUNT_OF(good); i++) {
            assert(harmony_writing_valid(&good[i]));
            assert(all_hold(&good[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_writing_check(&good[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_bass_motion(void) {
    HarmonyCadenceWriting reversed_pac = writing(HARMONY_MODE_MAJOR, PAC, 1, 5, 2, 1);
    HarmonyCadenceWriting reversed_hc = writing(HARMONY_MODE_MAJOR, HC, 5, 1, 1, 7);
    HarmonyCadenceWriting iac_hc_bass = writing(HARMONY_MODE_MAJOR, IAC, 1, 5, 2, 3);
    HarmonyCadenceWriting predominant_bass = writing(HARMONY_MODE_MAJOR, HC, 4, 5, 1, 7);
    assert(!harmony_writing_rule_holds(WRITING(BASS_MOTION), &reversed_pac));
    assert(!harmony_writing_rule_holds(WRITING(BASS_MOTION), &reversed_hc));
    assert(!harmony_writing_rule_holds(WRITING(BASS_MOTION), &iac_hc_bass));
    assert(!harmony_writing_rule_holds(WRITING(BASS_MOTION), &predominant_bass));
}

static void test_soprano_melody(void) {
    /* Each type's allowed motions are not allowed for the others. */
    HarmonyCadenceWriting pac_with_iac_melody = writing(HARMONY_MODE_MAJOR, PAC, 5, 1, 2, 3);
    HarmonyCadenceWriting pac_sol_sol = writing(HARMONY_MODE_MAJOR, PAC, 5, 1, 5, 5);
    HarmonyCadenceWriting iac_with_pac_melody = writing(HARMONY_MODE_MAJOR, IAC, 5, 1, 7, 1);
    HarmonyCadenceWriting hc_do_do = writing(HARMONY_MODE_MAJOR, HC, 1, 5, 1, 1);
    HarmonyCadenceWriting hc_reversed = writing(HARMONY_MODE_MAJOR, HC, 1, 5, 7, 1);
    assert(!harmony_writing_rule_holds(WRITING(SOPRANO_MELODY), &pac_with_iac_melody));
    assert(!harmony_writing_rule_holds(WRITING(SOPRANO_MELODY), &pac_sol_sol));
    assert(!harmony_writing_rule_holds(WRITING(SOPRANO_MELODY), &iac_with_pac_melody));
    assert(!harmony_writing_rule_holds(WRITING(SOPRANO_MELODY), &hc_do_do));
    assert(!harmony_writing_rule_holds(WRITING(SOPRANO_MELODY), &hc_reversed));
}

static void test_half_cadence_sol_sol_is_possible_but_uncommon(void) {
    HarmonyCadenceWriting hc = writing(HARMONY_MODE_MAJOR, HC, 1, 5, 5, 5);
    assert(harmony_writing_rule_holds(WRITING(SOPRANO_MELODY), &hc));
    assert(!harmony_writing_rule_holds(WRITING(COMMON_HC_SOPRANO), &hc));
    unsigned hard = 99, soft = 99;
    assert(harmony_writing_check(&hc, &hard, &soft));
    assert(hard == 0 && soft == (1u << WRITING(COMMON_HC_SOPRANO)));

    /* sol to sol in an imperfect authentic cadence is the usual one. */
    HarmonyCadenceWriting iac = writing(HARMONY_MODE_MAJOR, IAC, 5, 1, 5, 5);
    assert(harmony_writing_rule_holds(WRITING(COMMON_HC_SOPRANO), &iac));
}

static void test_minor_needs_the_leading_tone(void) {
    for (int type = CADENCE_TYPE_PERFECT_AUTHENTIC; type <= CADENCE_TYPE_HALF; type++) {
        HarmonyCadenceWriting minor = writing(HARMONY_MODE_MINOR, (CadenceType)type, 5, 1, 2, 1);
        minor.dominant_has_leading_tone = false;
        assert(!harmony_writing_rule_holds(WRITING(MINOR_LEADING_TONE), &minor));
        minor.dominant_has_leading_tone = true;
        assert(harmony_writing_rule_holds(WRITING(MINOR_LEADING_TONE), &minor));
    }
    /* In major the rule always holds, with or without the flag. */
    HarmonyCadenceWriting major = writing(HARMONY_MODE_MAJOR, PAC, 5, 1, 2, 1);
    major.dominant_has_leading_tone = false;
    assert(harmony_writing_rule_holds(WRITING(MINOR_LEADING_TONE), &major));
}

static void test_check_reports_each_failure(void) {
    HarmonyCadenceWriting bad = writing(HARMONY_MODE_MINOR, PAC, 1, 5, 1, 1);
    bad.dominant_has_leading_tone = false;
    unsigned hard = 0, soft = 99;
    assert(harmony_writing_check(&bad, &hard, &soft));
    assert(hard == ((1u << WRITING(BASS_MOTION)) | (1u << WRITING(SOPRANO_MELODY)) |
                    (1u << WRITING(MINOR_LEADING_TONE))));
    assert(soft == 0);
}

static void test_subverted_and_invalid_are_not_written(void) {
    HarmonyCadenceWriting subverted = writing(HARMONY_MODE_MAJOR, CADENCE_TYPE_SUBVERTED, 5, 1, 2, 1);
    HarmonyCadenceWriting bad_mode = writing(HARMONY_MODE_COUNT, PAC, 5, 1, 2, 1);
    HarmonyCadenceWriting bad_type = writing(HARMONY_MODE_MAJOR, CADENCE_TYPE_COUNT, 5, 1, 2, 1);
    HarmonyCadenceWriting bad_bass = writing(HARMONY_MODE_MAJOR, PAC, 0, 1, 2, 1);
    HarmonyCadenceWriting bad_soprano = writing(HARMONY_MODE_MAJOR, PAC, 5, 1, 2, 8);
    HarmonyCadenceWriting *bad[] = {&subverted, &bad_mode, &bad_type, &bad_bass, &bad_soprano};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_writing_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_WRITING_RULE_COUNT; rule++) {
            assert(!harmony_writing_rule_holds((HarmonyWritingRule)rule, bad[i]));
        }
        assert(!harmony_writing_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_writing_valid(NULL));
    HarmonyCadenceWriting ok = writing(HARMONY_MODE_MAJOR, PAC, 5, 1, 2, 1);
    assert(!harmony_writing_rule_holds(WRITING(RULE_COUNT), &ok));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_cadences();
    test_bass_motion();
    test_soprano_melody();
    test_half_cadence_sol_sol_is_possible_but_uncommon();
    test_minor_needs_the_leading_tone();
    test_check_reports_each_failure();
    test_subverted_and_invalid_are_not_written();
    printf("test_harmony_writing_rules: ok\n");
    return 0;
}
