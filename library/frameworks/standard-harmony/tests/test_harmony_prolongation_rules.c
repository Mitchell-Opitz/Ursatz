#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_prolongation_test_util.h"
#include "harmony_rule_check.h"

#define PRO(name) HARMONY_PROLONGATION_##name

static bool holds(HarmonyProlongationRule rule, const HarmonyTonicProlongation *p) {
    return harmony_prolongation_rule_holds(rule, p);
}

static bool all_hold(const HarmonyTonicProlongation *p) {
    for (int i = 0; i < HARMONY_PROLONGATION_RULE_COUNT; i++) {
        if (!holds((HarmonyProlongationRule)i, p)) {
            return false;
        }
    }
    return true;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(harmony_prolongation_rule, harmony_prolongation_constraint,
                                    HARMONY_PROLONGATION_RULE_COUNT);
    for (int i = 0; i < HARMONY_PROLONGATION_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(harmony_prolongation_constraint((HarmonyProlongationRule)i));
        assert(type == (i <= PRO(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    const char *const expected[HARMONY_PROLONGATION_RULE_COUNT] = {
        "progression",    "bass-line",         "chord-notes",
        "resolves",       "minor-leading-tone", "no-parallels",
        "avoid-v6-triad", "avoid-v43-to-root",  "middle-weaker-beat"};
    for (int i = 0; i < HARMONY_PROLONGATION_RULE_COUNT; i++) {
        assert(strcmp(harmony_prolongation_rule_name((HarmonyProlongationRule)i), expected[i]) == 0);
    }
    assert(harmony_prolongation_rule_name(PRO(RULE_COUNT)) == NULL);
}

static void test_structure_of_the_worked_progressions(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyTonicProlongation examples[] = {
            example_v65(m),         example_v6_triad(m), example_v43_to_first(m),
            example_v43_to_root(m), example_v42(m),      example_tenths(m),
        };
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            assert(harmony_prolongation_valid(&examples[i]));
            assert(holds(PRO(PROGRESSION), &examples[i]));
            assert(holds(PRO(BASS_LINE), &examples[i]));
            assert(holds(PRO(CHORD_NOTES), &examples[i]));
        }
    }
}

static void test_progression(void) {
    /* V6/5 goes to root position I, not I6. */
    HarmonyTonicProlongation p = example_v65(HARMONY_MODE_MAJOR);
    p.chords[2] = I_FIRST;
    assert(!holds(PRO(PROGRESSION), &p));
    /* The V is in root position. */
    p = example_v65(HARMONY_MODE_MAJOR);
    p.chords[1] = CH7(5);
    assert(!holds(PRO(PROGRESSION), &p));
    /* The first chord is not I. */
    p = example_v65(HARMONY_MODE_MAJOR);
    p.chords[0] = CH(4);
    assert(!holds(PRO(PROGRESSION), &p));
}

static void test_bass_line(void) {
    /* The bass has to be the line of the progression: ti for V6/5, then do. */
    HarmonyTonicProlongation p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_BASS] = -9; /* la, not ti */
    assert(!holds(PRO(BASS_LINE), &p));
    p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[2][VOICE_PART_BASS] = -5; /* mi, not do */
    assert(!holds(PRO(BASS_LINE), &p));
    p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[0][VOICE_PART_BASS] = -6; /* re, not do */
    assert(!holds(PRO(BASS_LINE), &p));
    /* V4/3 with ti in the bass (the V6/5 bass): the chord says re. */
    p = example_v43_to_first(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_BASS] = -8;
    assert(!holds(PRO(BASS_LINE), &p));
    /* An unclassifiable progression has no bass line. */
    p = example_v65(HARMONY_MODE_MAJOR);
    p.chords[2] = I_FIRST;
    assert(!holds(PRO(BASS_LINE), &p));
}

static void test_chord_notes(void) {
    HarmonyTonicProlongation p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[0][VOICE_PART_SOPRANO] = 3; /* fa in the tonic chord */
    assert(!holds(PRO(CHORD_NOTES), &p));

    /* An inverted V7 has sol, ti, re, and fa once each: sol twice and no fa is not. */
    p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_SOPRANO] = 4;
    assert(!holds(PRO(CHORD_NOTES), &p));
    p = example_v43_to_first(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_ALTO] = -1 + 7; /* ti twice */
    assert(!holds(PRO(CHORD_NOTES), &p));

    /* The V6 triad has sol, ti, and re only. */
    p = example_v6_triad(HARMONY_MODE_MAJOR);
    p.steps[1][VOICE_PART_SOPRANO] = 3; /* fa in a triad */
    assert(!holds(PRO(CHORD_NOTES), &p));

    /* The tonic chord has do and mi. */
    p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[2][VOICE_PART_SOPRANO] = 4; /* no mi in the last chord */
    assert(!holds(PRO(CHORD_NOTES), &p));
    p = example_v65(HARMONY_MODE_MAJOR);
    p.steps[0][VOICE_PART_ALTO] = -3;
    p.steps[0][VOICE_PART_TENOR] = -3;
    p.steps[0][VOICE_PART_SOPRANO] = 4;
    p.steps[0][VOICE_PART_BASS] = -7;
    assert(holds(PRO(CHORD_NOTES), &p) == false); /* sol, sol, sol, do: no mi */
}

static void test_structure_violations_are_hard(void) {
    HarmonyTonicProlongation p = example_v65(HARMONY_MODE_MAJOR);
    p.chords[2] = I_FIRST;
    unsigned hard = 0, soft = 99;
    assert(harmony_prolongation_check(&p, &hard, &soft));
    assert((hard & ((1u << PRO(PROGRESSION)) | (1u << PRO(BASS_LINE)) | (1u << PRO(CHORD_NOTES)))) ==
           ((1u << PRO(PROGRESSION)) | (1u << PRO(BASS_LINE)) | (1u << PRO(CHORD_NOTES))));
}

static void test_invalid_prolongation(void) {
    HarmonyTonicProlongation bad_mode = example_v65(HARMONY_MODE_COUNT);
    HarmonyTonicProlongation bad_chord = example_v65(HARMONY_MODE_MAJOR);
    bad_chord.chords[1] = CH(9);
    HarmonyTonicProlongation bad_beat = example_v65(HARMONY_MODE_MAJOR);
    bad_beat.beat[1] = BEAT_STRENGTH_COUNT;
    HarmonyTonicProlongation *bad[] = {&bad_mode, &bad_chord, &bad_beat};
    unsigned hard = 7, soft = 7;
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        assert(!harmony_prolongation_valid(bad[i]));
        for (int rule = 0; rule < HARMONY_PROLONGATION_RULE_COUNT; rule++) {
            assert(!holds((HarmonyProlongationRule)rule, bad[i]));
        }
        assert(!harmony_prolongation_check(bad[i], &hard, &soft));
    }
    assert(hard == 7 && soft == 7);
    assert(!harmony_prolongation_valid(NULL));
    HarmonyTonicProlongation ok = example_v65(HARMONY_MODE_MAJOR);
    assert(!holds(PRO(RULE_COUNT), &ok));
    (void)all_hold;
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_structure_of_the_worked_progressions();
    test_progression();
    test_bass_line();
    test_chord_notes();
    test_structure_violations_are_hard();
    test_invalid_prolongation();
    printf("test_harmony_prolongation_rules: ok\n");
    return 0;
}
