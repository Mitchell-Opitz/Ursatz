#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_mediant_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"
#include "harmony_eval_check.h"

/* Every call to the group's rule_holds below also runs the rule through constraint_evaluate. */
HARMONY_EVAL_TABLE(g_eval, "mediant", HARMONY_MEDIANT_RULE_COUNT);
#define harmony_mediant_rule_holds(which, subject) \
    HARMONY_EVAL_HOLDS(g_eval, harmony_mediant_constraint, harmony_mediant_rule_holds, HarmonyMediant, which, subject)

#define MED(name) HARMONY_MEDIANT_##name
#define BIT(rule) (1u << (rule))

static HarmonyMediant make(HarmonyMode mode, HarmonyChord c, const int s[3][4]) {
    HarmonyMediant m;
    memset(&m, 0, sizeof(m));
    m.mode = mode;
    m.chords[0] = CH(1);
    m.chords[1] = CH(3);
    m.chords[2] = c;
    memcpy(m.steps, s, sizeof(m.steps));
    return m;
}

/* I iii IV over do mi fa: the soprano do ti la, the leading tone descending to la. */
static HarmonyMediant example_through(HarmonyMode mode) {
    static const int s[3][4] = {{7, 4, 2, -7}, {6, 4, 2, -5}, {5, 3, 0, -4}};
    return make(mode, CH(4), s);
}

/* I iii V over do mi sol: the ti stays as a common tone into V. */
static HarmonyMediant example_direct(HarmonyMode mode) {
    static const int s[3][4] = {{7, 4, 2, -7}, {6, 4, 2, -5}, {6, 4, 1, -3}};
    return make(mode, CH(5), s);
}

static bool holds(HarmonyMediantRule rule, const HarmonyMediant *m) {
    return harmony_mediant_rule_holds(rule, m);
}

static void expect_breaks(const HarmonyMediant *m, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_mediant_check(m, &h, &s));
    assert(h == hard && s == soft);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_mediant_rule, harmony_mediant_constraint, HARMONY_MEDIANT_RULE_COUNT);
    for (int i = 0; i < HARMONY_MEDIANT_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_mediant_constraint((HarmonyMediantRule)i));
        assert(type == (i <= MED(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_MEDIANT_RULE_COUNT] = {
        "progression",  "bass-line",     "chord-notes",         "descending-leading-tone",
        "no-parallels", "root-position", "through-predominant", "avoid-v-in-minor"};
    for (int i = 0; i < HARMONY_MEDIANT_RULE_COUNT; i++) {
        assert(strcmp(harmony_mediant_rule_name((HarmonyMediantRule)i), expected[i]) == 0);
    }
    assert(harmony_mediant_rule_name(MED(RULE_COUNT)) == NULL);
}

static void test_the_worked_example_through_a_predominant_breaks_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMediant m = example_through((HarmonyMode)mode);
        for (int rule = 0; rule < HARMONY_MEDIANT_RULE_COUNT; rule++) {
            assert(holds((HarmonyMediantRule)rule, &m));
        }
        expect_breaks(&m, 0, 0);
    }
}

static void test_going_directly_to_v_breaks_only_soft_rules(void) {
    HarmonyMediant major = example_direct(HARMONY_MODE_MAJOR);
    HarmonyMediant minor = example_direct(HARMONY_MODE_MINOR);
    expect_breaks(&major, 0, BIT(MED(THROUGH_PREDOMINANT)));
    assert(!holds(MED(THROUGH_PREDOMINANT), &major));
    expect_breaks(&minor, 0, BIT(MED(THROUGH_PREDOMINANT)) | BIT(MED(AVOID_V_IN_MINOR)));
    assert(!holds(MED(AVOID_V_IN_MINOR), &minor));
}

static void test_progression(void) {
    HarmonyMediant m = example_through(HARMONY_MODE_MAJOR);
    m.chords[1] = CH(2);
    assert(!holds(MED(PROGRESSION), &m));
    assert(!holds(MED(BASS_LINE), &m));
}

static void test_iii_in_first_inversion_breaks_only_the_soft_rule(void) {
    HarmonyMediant m = example_through(HARMONY_MODE_MAJOR);
    m.chords[1] = CHI(3, 1);
    m.steps[1][VOICE_PART_BASS] = -3; /* sol, the third of iii */
    assert(holds(MED(PROGRESSION), &m));
    assert(holds(MED(BASS_LINE), &m));
    assert(!holds(MED(ROOT_POSITION), &m));
}

static void test_each_chord_has_its_own_bass(void) {
    HarmonyMediant m = example_direct(HARMONY_MODE_MAJOR);
    m.steps[1][VOICE_PART_BASS] = -3; /* sol in the bass of a root position iii */
    assert(!holds(MED(BASS_LINE), &m));
}

static void test_each_chord_has_its_own_notes(void) {
    HarmonyMediant m = example_through(HARMONY_MODE_MAJOR);
    m.steps[1][VOICE_PART_ALTO] = 5; /* la is not in iii */
    assert(!holds(MED(CHORD_NOTES), &m));
}

static void test_the_leading_tone_descends(void) {
    HarmonyMediant not_from_do = example_through(HARMONY_MODE_MAJOR);
    not_from_do.steps[0][VOICE_PART_SOPRANO] = 9; /* re over the I, not do */
    HarmonyMediant stays = example_through(HARMONY_MODE_MAJOR);
    stays.steps[2][VOICE_PART_SOPRANO] = 6; /* ti does not go to la */
    HarmonyMediant no_ti = example_through(HARMONY_MODE_MAJOR);
    no_ti.steps[1][VOICE_PART_SOPRANO] = 4; /* sol: no ti over iii */
    HarmonyMediant moves_into_v = example_direct(HARMONY_MODE_MAJOR);
    moves_into_v.steps[2][VOICE_PART_SOPRANO] = 7; /* ti does not stay */
    assert(!holds(MED(DESCENDING_LEADING_TONE), &not_from_do));
    assert(!holds(MED(DESCENDING_LEADING_TONE), &stays));
    assert(!holds(MED(DESCENDING_LEADING_TONE), &no_ti));
    assert(!holds(MED(DESCENDING_LEADING_TONE), &moves_into_v));
}

static void test_no_parallel_octaves(void) {
    HarmonyMediant m = example_through(HARMONY_MODE_MAJOR);
    m.steps[0][VOICE_PART_ALTO] = 0; /* do over the bass do */
    m.steps[1][VOICE_PART_ALTO] = 2; /* mi over the bass mi, both up */
    assert(!holds(MED(NO_PARALLELS), &m));
}

static void test_invalid_subject(void) {
    HarmonyMediant bad_chord = example_through(HARMONY_MODE_MAJOR);
    bad_chord.chords[2] = CH(9);
    HarmonyMediant bad_mode = example_through(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyMediant good = example_through(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_mediant_valid(NULL));
    assert(!harmony_mediant_valid(&bad_chord));
    assert(!harmony_mediant_valid(&bad_mode));
    assert(!harmony_mediant_check(&bad_chord, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_mediant_check(&good, NULL, &soft));
    assert(!harmony_mediant_check(&good, &hard, NULL));
    assert(!holds(MED(PROGRESSION), &bad_chord));
    assert(!holds(MED(RULE_COUNT), &good));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_the_worked_example_through_a_predominant_breaks_no_rule();
    test_going_directly_to_v_breaks_only_soft_rules();
    test_progression();
    test_iii_in_first_inversion_breaks_only_the_soft_rule();
    test_each_chord_has_its_own_bass();
    test_each_chord_has_its_own_notes();
    test_the_leading_tone_descends();
    test_no_parallel_octaves();
    test_invalid_subject();
    printf("test_harmony_mediant_rules: ok\n");
    return 0;
}
