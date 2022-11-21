#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_applied_rules.h"
#include "harmony_rule_check.h"
#include "harmony_test_util.h"

#define AR(name) HARMONY_APPLIED_##name
#define BIT(rule) (1u << (rule))
#define SD APPLIED_CHORD_KIND_SECONDARY_DOMINANT
#define SLT APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE
#define S VOICE_PART_SOPRANO
#define A VOICE_PART_ALTO
#define T VOICE_PART_TENOR
#define B VOICE_PART_BASS

static HarmonyAppliedMotion make(HarmonyMode mode, HarmonyAppliedChord applied, HarmonyChord next,
                                 const int steps[2][4], const int alter[2][4]) {
    HarmonyAppliedMotion m;
    memset(&m, 0, sizeof(m));
    m.mode = mode;
    m.applied = applied;
    m.next = next;
    memcpy(m.steps, steps, sizeof(m.steps));
    memcpy(m.alter, alter, sizeof(m.alter));
    return m;
}

/* V7/V to V in C major: D F# A C to G B D G. The F sharp (the secondary leading tone) goes up to
   G, the C (the secondary seventh) down to B. */
static HarmonyAppliedMotion example_v7_of_v(HarmonyMode mode) {
    static const int steps[2][4] = {{10, 7, 5, -6}, {11, 6, 4, -3}};
    static const int alter[2][4] = {{1, 0, 0, 0}, {0, 0, 0, 0}};
    return make(mode, (HarmonyAppliedChord){SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 5}, CH(5), steps, alter);
}

/* viio7/V to V: F# A C Eb to G B D. The Eb (the secondary seventh) goes down to D. */
static HarmonyAppliedMotion example_viio7_of_v(HarmonyMode mode) {
    static const int steps[2][4] = {{10, 7, 5, -5}, {11, 6, 4, -6}};
    static const int alter[2][4] = {{1, 0, 0, -1}, {0, 0, 0, 0}};
    return make(mode, (HarmonyAppliedChord){SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 5}, CH(5), steps, alter);
}

/* V7/III to III in C minor, with no accidentals: Bb D F Ab to Eb G Bb. */
static HarmonyAppliedMotion example_v7_of_iii_minor(void) {
    static const int steps[2][4] = {{8, 5, 3, -1}, {9, 4, 4, -5}};
    static const int alter[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
    return make(HARMONY_MODE_MINOR, (HarmonyAppliedChord){SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 3}, CH(3), steps,
                alter);
}

static bool holds(HarmonyAppliedRule rule, const HarmonyAppliedMotion *m) {
    return harmony_applied_rule_holds(rule, m);
}

static void expect_breaks(const HarmonyAppliedMotion *m, unsigned hard, unsigned soft) {
    unsigned h = 99, s = 99;
    assert(harmony_applied_check(m, &h, &s));
    assert(h == hard && s == soft);
}

static void test_pitches(void) {
    assert(harmony_applied_pitch(HARMONY_MODE_MAJOR, 0, 0) == 0);
    assert(harmony_applied_pitch(HARMONY_MODE_MAJOR, 10, 1) == 18);
    assert(harmony_applied_pitch(HARMONY_MODE_MAJOR, -6, 0) == -10);
    assert(harmony_applied_pitch(HARMONY_MODE_MAJOR, -5, -1) == -9);
    assert(harmony_applied_pitch(HARMONY_MODE_MAJOR, -7, 0) == -12);
    assert(harmony_applied_pitch(HARMONY_MODE_MAJOR, -8, 0) == -13);
    assert(harmony_applied_pitch(HARMONY_MODE_MINOR, 6, 0) == 10);
    assert(harmony_applied_pitch(HARMONY_MODE_MINOR, -1, 0) == -2);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_applied_rule, harmony_applied_constraint, HARMONY_APPLIED_RULE_COUNT);
    for (int i = 0; i < HARMONY_APPLIED_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_applied_constraint((HarmonyAppliedRule)i));
        assert(type == (i <= AR(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_APPLIED_RULE_COUNT] = {
        "progression",  "chord-notes",           "leading-tone-resolves", "seventh-resolves",
        "no-parallels", "avoid-half-diminished", "chromatic"};
    for (int i = 0; i < HARMONY_APPLIED_RULE_COUNT; i++) {
        assert(strcmp(harmony_applied_rule_name((HarmonyAppliedRule)i), expected[i]) == 0);
    }
    assert(harmony_applied_rule_name(AR(RULE_COUNT)) == NULL);
}

static void test_the_worked_examples_break_no_rule(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyAppliedMotion examples[] = {example_v7_of_v((HarmonyMode)mode), example_viio7_of_v((HarmonyMode)mode)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            for (int rule = 0; rule < HARMONY_APPLIED_RULE_COUNT; rule++) {
                assert(holds((HarmonyAppliedRule)rule, &examples[i]));
            }
            expect_breaks(&examples[i], 0, 0);
        }
    }
}

static void test_the_chord_after_is_the_target(void) {
    HarmonyAppliedMotion tonic = example_v7_of_v(HARMONY_MODE_MAJOR);
    tonic.next = CH(1);
    HarmonyAppliedMotion inverted = example_v7_of_v(HARMONY_MODE_MAJOR);
    inverted.next = CHI(5, 1);
    HarmonyAppliedMotion seventh = example_v7_of_v(HARMONY_MODE_MAJOR);
    seventh.next = CH7(5);
    HarmonyAppliedMotion cadential = example_v7_of_v(HARMONY_MODE_MAJOR);
    cadential.next = CADENTIAL_CHORD;
    assert(!holds(AR(PROGRESSION), &tonic));
    assert(holds(AR(PROGRESSION), &inverted));
    assert(holds(AR(PROGRESSION), &seventh));
    assert(holds(AR(PROGRESSION), &cadential)); /* the cadential 6/4 embellishes the target V */
    HarmonyAppliedMotion of_vi = example_v7_of_v(HARMONY_MODE_MAJOR);
    of_vi.applied.target = 6;
    of_vi.next = CADENTIAL_CHORD;
    assert(!holds(AR(PROGRESSION), &of_vi));
}

static void test_each_voice_is_a_note_of_the_applied_chord(void) {
    HarmonyAppliedMotion foreign = example_v7_of_v(HARMONY_MODE_MAJOR);
    foreign.steps[0][T] = 6; /* B is not in D7 */
    HarmonyAppliedMotion no_seventh = example_v7_of_v(HARMONY_MODE_MAJOR);
    no_seventh.steps[0][A] = 5; /* A in the alto too: no C */
    HarmonyAppliedMotion unaltered = example_v7_of_v(HARMONY_MODE_MAJOR);
    unaltered.alter[0][S] = 0; /* F natural: no leading tone */
    assert(!holds(AR(CHORD_NOTES), &foreign));
    assert(!holds(AR(CHORD_NOTES), &no_seventh));
    assert(!holds(AR(CHORD_NOTES), &unaltered));
}

static void test_the_secondary_leading_tone_resolves_up(void) {
    HarmonyAppliedMotion stays = example_v7_of_v(HARMONY_MODE_MAJOR);
    stays.steps[1][S] = 10;
    stays.alter[1][S] = 1;
    HarmonyAppliedMotion falls = example_v7_of_v(HARMONY_MODE_MAJOR);
    falls.steps[1][S] = 10; /* F natural, down a semitone */
    HarmonyAppliedMotion into_seventh = falls;
    into_seventh.next = CH7(5);
    assert(!holds(AR(LEADING_TONE_RESOLVES), &stays));
    assert(!holds(AR(LEADING_TONE_RESOLVES), &falls));
    assert(holds(AR(LEADING_TONE_RESOLVES), &into_seventh)); /* D7 G7 C: F sharp, F natural, E */
}

static void test_the_root_of_a_leading_tone_chord_is_its_leading_tone(void) {
    HarmonyAppliedMotion m = example_viio7_of_v(HARMONY_MODE_MAJOR);
    m.steps[1][S] = 12; /* F sharp up a whole tone */
    assert(!holds(AR(LEADING_TONE_RESOLVES), &m));
}

static void test_the_secondary_seventh_resolves_down_by_step(void) {
    HarmonyAppliedMotion stays = example_v7_of_v(HARMONY_MODE_MAJOR);
    stays.steps[1][A] = 7;
    HarmonyAppliedMotion leaps = example_v7_of_v(HARMONY_MODE_MAJOR);
    leaps.steps[1][A] = 4;
    HarmonyAppliedMotion whole_tone = example_v7_of_v(HARMONY_MODE_MAJOR);
    whole_tone.steps[1][A] = 5; /* C down to A: a whole tone is a step too... */
    whole_tone.steps[1][A] = 6;
    whole_tone.alter[1][A] = -1; /* ...C down to Bb */
    assert(!holds(AR(SEVENTH_RESOLVES), &stays));
    assert(!holds(AR(SEVENTH_RESOLVES), &leaps));
    assert(holds(AR(SEVENTH_RESOLVES), &whole_tone));
}

static void test_a_triad_has_no_seventh_to_resolve(void) {
    HarmonyAppliedMotion m = example_v7_of_v(HARMONY_MODE_MAJOR);
    m.applied.quality = HARMONY_APPLIED_MAJOR_TRIAD;
    m.steps[0][A] = 5; /* A, not C */
    m.steps[1][A] = 4;
    assert(holds(AR(SEVENTH_RESOLVES), &m));
    assert(holds(AR(CHORD_NOTES), &m));
}

static void test_no_parallel_fifths(void) {
    HarmonyAppliedMotion m = example_v7_of_v(HARMONY_MODE_MAJOR);
    m.steps[1][T] = 8; /* A up to D, with the bass up to G: fifths */
    assert(!holds(AR(NO_PARALLELS), &m));
}

static void test_the_half_diminished_chord_is_rare(void) {
    HarmonyAppliedMotion m = example_viio7_of_v(HARMONY_MODE_MAJOR);
    m.applied.quality = HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH;
    m.alter[0][B] = 0; /* E natural, the seventh of the half-diminished chord */
    assert(holds(AR(CHORD_NOTES), &m));
    expect_breaks(&m, 0, BIT(AR(AVOID_HALF_DIMINISHED)));
}

static void test_an_applied_chord_without_accidentals_is_rare(void) {
    HarmonyAppliedMotion m = example_v7_of_iii_minor();
    for (int rule = 0; rule <= AR(NO_PARALLELS); rule++) {
        assert(holds((HarmonyAppliedRule)rule, &m));
    }
    expect_breaks(&m, 0, BIT(AR(CHROMATIC)));
}

static void test_invalid_subject(void) {
    HarmonyAppliedMotion bad_target = example_v7_of_v(HARMONY_MODE_MAJOR);
    bad_target.applied.target = 1;
    HarmonyAppliedMotion bad_next = example_v7_of_v(HARMONY_MODE_MAJOR);
    bad_next.next = CH(9);
    HarmonyAppliedMotion bad_mode = example_v7_of_v(HARMONY_MODE_MAJOR);
    bad_mode.mode = HARMONY_MODE_COUNT;
    HarmonyAppliedMotion good = example_v7_of_v(HARMONY_MODE_MAJOR);
    unsigned hard = 7, soft = 7;
    assert(!harmony_applied_motion_valid(NULL));
    assert(!harmony_applied_motion_valid(&bad_target));
    assert(!harmony_applied_motion_valid(&bad_next));
    assert(!harmony_applied_motion_valid(&bad_mode));
    assert(!harmony_applied_check(&bad_target, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!harmony_applied_check(&good, NULL, &soft));
    assert(!harmony_applied_check(&good, &hard, NULL));
    assert(!holds(AR(PROGRESSION), &bad_target));
    assert(!holds(AR(RULE_COUNT), &good));
}

int main(void) {
    test_pitches();
    test_rules_reference_framework();
    test_names();
    test_the_worked_examples_break_no_rule();
    test_the_chord_after_is_the_target();
    test_each_voice_is_a_note_of_the_applied_chord();
    test_the_secondary_leading_tone_resolves_up();
    test_the_root_of_a_leading_tone_chord_is_its_leading_tone();
    test_the_secondary_seventh_resolves_down_by_step();
    test_a_triad_has_no_seventh_to_resolve();
    test_no_parallel_fifths();
    test_the_half_diminished_chord_is_rare();
    test_an_applied_chord_without_accidentals_is_rare();
    test_invalid_subject();
    printf("test_harmony_applied_rules: ok\n");
    return 0;
}
