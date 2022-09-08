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
        "avoid-v6-triad", "avoid-v43-to-root",  "middle-weaker-beat",
        "avoid-half-diminished", "re-above-la"};
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

static void test_the_worked_progressions_resolve(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyTonicProlongation examples[] = {
            example_v65(m),         example_v6_triad(m), example_v43_to_first(m),
            example_v43_to_root(m), example_v42(m),      example_tenths(m),
        };
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            assert(holds(PRO(RESOLVES), &examples[i]));
            assert(holds(PRO(MINOR_LEADING_TONE), &examples[i]));
            assert(holds(PRO(NO_PARALLELS), &examples[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_prolongation_check(&examples[i], &hard, &soft));
            assert(hard == 0);
        }
    }
}

static void test_ti_resolves_up_to_do(void) {
    /* The bass ti of V6/5 has to go up to do. */
    HarmonyTonicProlongation stays = example_v65(HARMONY_MODE_MAJOR);
    stays.steps[2][VOICE_PART_BASS] = -8;
    HarmonyTonicProlongation down = example_v65(HARMONY_MODE_MAJOR);
    down.steps[2][VOICE_PART_BASS] = -9;
    HarmonyTonicProlongation leaps = example_v65(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_BASS] = -14; /* do an octave lower */
    assert(!holds(PRO(RESOLVES), &stays));
    assert(!holds(PRO(RESOLVES), &down));
    assert(!holds(PRO(RESOLVES), &leaps));
}

static void test_the_seventh_resolves_down_to_mi(void) {
    /* fa goes down a step to mi, in an upper voice or in the bass (V4/2). */
    HarmonyTonicProlongation up = example_v65(HARMONY_MODE_MAJOR);
    up.steps[2][VOICE_PART_SOPRANO] = 4; /* fa up to sol, with no tenths */
    HarmonyTonicProlongation stays = example_v65(HARMONY_MODE_MAJOR);
    stays.steps[2][VOICE_PART_SOPRANO] = 3;
    HarmonyTonicProlongation bass_up = example_v42(HARMONY_MODE_MAJOR);
    bass_up.steps[2][VOICE_PART_BASS] = -3; /* the bass fa up to sol */
    HarmonyTonicProlongation bass_stays = example_v42(HARMONY_MODE_MAJOR);
    bass_stays.steps[2][VOICE_PART_BASS] = -4;
    assert(!holds(PRO(RESOLVES), &up));
    assert(!holds(PRO(RESOLVES), &stays));
    assert(!holds(PRO(RESOLVES), &bass_up));
    assert(!holds(PRO(RESOLVES), &bass_stays));
}

static void test_re_goes_a_step(void) {
    HarmonyTonicProlongation leaps = example_v65(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_ALTO] = -3; /* re leaps down to sol */
    HarmonyTonicProlongation stays = example_v65(HARMONY_MODE_MAJOR);
    stays.steps[2][VOICE_PART_ALTO] = 1;
    assert(!holds(PRO(RESOLVES), &leaps));
    assert(!holds(PRO(RESOLVES), &stays));
    /* The bass re of V4/3 goes to either do or mi. */
    HarmonyTonicProlongation to_mi = example_v43_to_first(HARMONY_MODE_MAJOR);
    HarmonyTonicProlongation to_do = example_v43_to_root(HARMONY_MODE_MAJOR);
    assert(holds(PRO(RESOLVES), &to_mi));
    assert(holds(PRO(RESOLVES), &to_do));
    HarmonyTonicProlongation bass_leaps = example_v43_to_first(HARMONY_MODE_MAJOR);
    bass_leaps.steps[2][VOICE_PART_BASS] = -3;
    assert(!holds(PRO(RESOLVES), &bass_leaps));
}

static void test_the_exception_for_parallel_tenths(void) {
    /* In do re mi, an upper voice on mi fa sol, parallel tenths with the bass, takes
       fa up to sol. */
    HarmonyTonicProlongation tenths = example_tenths(HARMONY_MODE_MAJOR);
    assert(holds(PRO(RESOLVES), &tenths));
    /* Any upper voice will do. */
    HarmonyTonicProlongation alto = example_v43_to_first(HARMONY_MODE_MAJOR);
    alto.steps[2][VOICE_PART_ALTO] = 4; /* mi fa sol in the alto: 2, 3, 4 */
    assert(holds(PRO(RESOLVES), &alto));

    /* Not a tenth the whole way: the first chord's note is not mi. */
    HarmonyTonicProlongation not_tenths = example_tenths(HARMONY_MODE_MAJOR);
    not_tenths.steps[0][VOICE_PART_SOPRANO] = 0;
    assert(!holds(PRO(RESOLVES), &not_tenths));
    /* Not a tenth: a seventeenth (two octaves and a third) the whole way. */
    HarmonyTonicProlongation compound = example_tenths(HARMONY_MODE_MAJOR);
    for (int i = 0; i < 3; i++) {
        compound.steps[i][VOICE_PART_SOPRANO] += 7;
    }
    assert(!holds(PRO(RESOLVES), &compound));
    /* Only in do re mi: do re do has no tenths with a bass that returns to do. */
    HarmonyTonicProlongation do_re_do = example_v43_to_root(HARMONY_MODE_MAJOR);
    do_re_do.steps[2][VOICE_PART_ALTO] = 4;
    assert(!holds(PRO(RESOLVES), &do_re_do));
    /* Only for V4/3: the V6/5 has no such exception. */
    HarmonyTonicProlongation v65 = example_v65(HARMONY_MODE_MAJOR);
    v65.steps[0][VOICE_PART_SOPRANO] = 2;
    v65.steps[2][VOICE_PART_SOPRANO] = 4;
    assert(!holds(PRO(RESOLVES), &v65));
}

static void test_minor_needs_the_leading_tone(void) {
    HarmonyTonicProlongation minor = example_v65(HARMONY_MODE_MINOR);
    minor.dominant_has_leading_tone = false;
    assert(!holds(PRO(MINOR_LEADING_TONE), &minor));
    minor.dominant_has_leading_tone = true;
    assert(holds(PRO(MINOR_LEADING_TONE), &minor));
    HarmonyTonicProlongation major = example_v65(HARMONY_MODE_MAJOR);
    major.dominant_has_leading_tone = false;
    assert(holds(PRO(MINOR_LEADING_TONE), &major));
}

static void test_no_parallel_fifths_or_octaves(void) {
    /* The tenor and bass, an octave apart, both fall a step into the V. */
    HarmonyTonicProlongation octaves = example_v65(HARMONY_MODE_MAJOR);
    octaves.steps[1][VOICE_PART_TENOR] = -1;
    assert(!holds(PRO(NO_PARALLELS), &octaves));
    /* The alto and tenor, a fifth apart, both fall a step out of the V. */
    HarmonyTonicProlongation fifths = example_v65(HARMONY_MODE_MAJOR);
    fifths.steps[2][VOICE_PART_TENOR] = -4;
    assert(!holds(PRO(NO_PARALLELS), &fifths));

    unsigned hard = 0, soft = 99;
    assert(harmony_prolongation_check(&octaves, &hard, &soft));
    assert((hard & (1u << PRO(NO_PARALLELS))) != 0);
}

static unsigned soft_of(const HarmonyTonicProlongation *p) {
    unsigned hard = 99, soft = 99;
    assert(harmony_prolongation_check(p, &hard, &soft));
    assert(hard == 0);
    return soft;
}

static void test_the_usual_progressions_break_no_preference(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyTonicProlongation v65 = example_v65(m);
        HarmonyTonicProlongation v43 = example_v43_to_first(m);
        HarmonyTonicProlongation v42 = example_v42(m);
        HarmonyTonicProlongation tenths = example_tenths(m);
        assert(soft_of(&v65) == 0);
        assert(soft_of(&v43) == 0);
        assert(soft_of(&v42) == 0);
        assert(soft_of(&tenths) == 0);
    }
}

static void test_v6_triad_is_less_common(void) {
    HarmonyTonicProlongation triad = example_v6_triad(HARMONY_MODE_MAJOR);
    assert(!holds(PRO(AVOID_V6_TRIAD), &triad));
    assert(soft_of(&triad) == (1u << PRO(AVOID_V6_TRIAD)));
    HarmonyTonicProlongation v65 = example_v65(HARMONY_MODE_MAJOR);
    assert(holds(PRO(AVOID_V6_TRIAD), &v65));
}

static void test_v43_to_root_tonic_is_less_common(void) {
    HarmonyTonicProlongation to_root = example_v43_to_root(HARMONY_MODE_MAJOR);
    assert(!holds(PRO(AVOID_V43_TO_ROOT), &to_root));
    assert(soft_of(&to_root) == (1u << PRO(AVOID_V43_TO_ROOT)));
    /* do re mi is the usual one, and the other progressions are not affected. */
    HarmonyTonicProlongation to_first = example_v43_to_first(HARMONY_MODE_MAJOR);
    HarmonyTonicProlongation v65 = example_v65(HARMONY_MODE_MAJOR);
    HarmonyTonicProlongation v42 = example_v42(HARMONY_MODE_MAJOR);
    assert(holds(PRO(AVOID_V43_TO_ROOT), &to_first));
    assert(holds(PRO(AVOID_V43_TO_ROOT), &v65));
    assert(holds(PRO(AVOID_V43_TO_ROOT), &v42));
}

static void test_the_v_is_on_a_weaker_beat(void) {
    const BeatStrength S = BEAT_STRENGTH_STRONG, M = BEAT_STRENGTH_MEDIUM, W = BEAT_STRENGTH_WEAK;
    const BeatStrength good[][3] = {{S, W, S}, {S, M, S}, {S, W, M}, {M, W, M}};
    for (size_t i = 0; i < COUNT_OF(good); i++) {
        HarmonyTonicProlongation p = example_v65(HARMONY_MODE_MAJOR);
        for (int c = 0; c < 3; c++) {
            p.beat[c] = good[i][c];
        }
        assert(holds(PRO(MIDDLE_WEAKER_BEAT), &p));
    }
    const BeatStrength bad[][3] = {{S, S, S}, {W, S, W}, {S, W, W}, {W, W, S}, {M, M, S}, {S, M, M}};
    for (size_t i = 0; i < COUNT_OF(bad); i++) {
        HarmonyTonicProlongation p = example_v65(HARMONY_MODE_MAJOR);
        for (int c = 0; c < 3; c++) {
            p.beat[c] = bad[i][c];
        }
        assert(!holds(PRO(MIDDLE_WEAKER_BEAT), &p));
        /* It is advice: nothing hard breaks. */
        assert(soft_of(&p) == (1u << PRO(MIDDLE_WEAKER_BEAT)));
    }
}

static void test_leading_tone_chords_structure_and_resolution(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyTonicProlongation examples[] = {example_vii7(m), example_vii6_triad(m), example_vii65(m),
                                               example_vii43(m)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            assert(harmony_prolongation_valid(&examples[i]));
            assert(holds(PRO(PROGRESSION), &examples[i]));
            assert(holds(PRO(BASS_LINE), &examples[i]));
            assert(holds(PRO(CHORD_NOTES), &examples[i]));
            assert(holds(PRO(RESOLVES), &examples[i]));
            assert(holds(PRO(MINOR_LEADING_TONE), &examples[i]));
            assert(holds(PRO(NO_PARALLELS), &examples[i]));
            unsigned hard = 99, soft = 99;
            assert(harmony_prolongation_check(&examples[i], &hard, &soft));
            assert(hard == 0 && soft == 0);
        }
    }
}

static void test_leading_tone_chord_notes(void) {
    /* vii7 is ti, re, fa, and le (the sixth degree) once each. */
    HarmonyTonicProlongation twice_ti = example_vii7(HARMONY_MODE_MAJOR);
    twice_ti.steps[1][VOICE_PART_TENOR] = -1; /* ti again, no le */
    HarmonyTonicProlongation no_re = example_vii7(HARMONY_MODE_MAJOR);
    no_re.steps[1][VOICE_PART_ALTO] = 4; /* sol, not re */
    assert(!holds(PRO(CHORD_NOTES), &twice_ti));
    assert(!holds(PRO(CHORD_NOTES), &no_re));
    /* The triad vii6 is ti, re, and fa, with ti and re present. */
    HarmonyTonicProlongation with_sol = example_vii6_triad(HARMONY_MODE_MAJOR);
    with_sol.steps[1][VOICE_PART_SOPRANO] = 4;
    HarmonyTonicProlongation no_ti = example_vii6_triad(HARMONY_MODE_MAJOR);
    no_ti.steps[1][VOICE_PART_TENOR] = 3; /* fa, so no ti */
    assert(!holds(PRO(CHORD_NOTES), &with_sol));
    assert(!holds(PRO(CHORD_NOTES), &no_ti));
    /* The triad is not the seventh chord: le in it is a foreign note. */
    HarmonyTonicProlongation le_in_triad = example_vii6_triad(HARMONY_MODE_MAJOR);
    le_in_triad.steps[1][VOICE_PART_ALTO] = 5;
    assert(!holds(PRO(CHORD_NOTES), &le_in_triad));
}

static void test_le_falls_to_sol(void) {
    /* The seventh of the leading-tone chord, le or la, falls a step to sol. */
    HarmonyTonicProlongation stays = example_vii7(HARMONY_MODE_MAJOR);
    stays.steps[2][VOICE_PART_TENOR] = -2;
    HarmonyTonicProlongation rises = example_vii7(HARMONY_MODE_MAJOR);
    rises.steps[2][VOICE_PART_TENOR] = -1;
    HarmonyTonicProlongation leaps = example_vii7(HARMONY_MODE_MAJOR);
    leaps.steps[2][VOICE_PART_TENOR] = -5;
    assert(!holds(PRO(RESOLVES), &stays));
    assert(!holds(PRO(RESOLVES), &rises));
    assert(!holds(PRO(RESOLVES), &leaps));
    /* The other tendencies are the V7's: ti up, re down, fa down. */
    HarmonyTonicProlongation fa_up = example_vii7(HARMONY_MODE_MAJOR);
    fa_up.steps[2][VOICE_PART_SOPRANO] = 4;
    HarmonyTonicProlongation ti_stays = example_vii7(HARMONY_MODE_MAJOR);
    ti_stays.steps[2][VOICE_PART_BASS] = -8;
    HarmonyTonicProlongation re_leaps = example_vii7(HARMONY_MODE_MAJOR);
    re_leaps.steps[2][VOICE_PART_ALTO] = -3;
    assert(!holds(PRO(RESOLVES), &fa_up));
    assert(!holds(PRO(RESOLVES), &ti_stays));
    assert(!holds(PRO(RESOLVES), &re_leaps));
}

static void test_minor_leading_tone_chord_needs_ti(void) {
    /* In minor the chord is built on ti, not te: without its flag it is VII. */
    HarmonyTonicProlongation minor = example_vii7(HARMONY_MODE_MINOR);
    assert(minor.chords[1].leading_tone);
    assert(holds(PRO(MINOR_LEADING_TONE), &minor));
    minor.chords[1].leading_tone = false;
    assert(!holds(PRO(MINOR_LEADING_TONE), &minor));
    minor.chords[1].leading_tone = true;
    minor.dominant_has_leading_tone = false;
    assert(!holds(PRO(MINOR_LEADING_TONE), &minor));
    /* In major the leading tone is implied. */
    HarmonyTonicProlongation major = example_vii7(HARMONY_MODE_MAJOR);
    assert(!major.chords[1].leading_tone);
    assert(holds(PRO(MINOR_LEADING_TONE), &major));
}

static void test_re_and_le_are_a_diminished_fifth(void) {
    /* In the fully diminished chord, re with le above it is a diminished fifth: the soprano
       (le) and the bass (re) of vii6/5 both go up a step into it from a perfect fifth
       (sol over do), and down to a perfect fifth (sol over do), with no parallel fifths. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyTonicProlongation p = example_vii65((HarmonyMode)mode);
        assert(holds(PRO(NO_PARALLELS), &p));
    }
    HarmonyTonicProlongation vii43 = example_vii43(HARMONY_MODE_MAJOR);
    assert(holds(PRO(NO_PARALLELS), &vii43));
    HarmonyTonicProlongation vii7 = example_vii7(HARMONY_MODE_MAJOR);
    assert(holds(PRO(NO_PARALLELS), &vii7));
}

static void test_half_diminished_has_perfect_fifth(void) {
    /* The same notes, with la for le: re and la are a perfect fifth, and sol over do in
       the first chord goes to la over re (parallel fifths between the soprano and the
       bass). */
    HarmonyTonicProlongation half = example_half_diminished_65();
    assert(holds(PRO(PROGRESSION), &half));
    assert(holds(PRO(CHORD_NOTES), &half));
    assert(holds(PRO(RESOLVES), &half));
    assert(!holds(PRO(NO_PARALLELS), &half));
    unsigned hard = 0, soft = 99;
    assert(harmony_prolongation_check(&half, &hard, &soft));
    assert(hard == (1u << PRO(NO_PARALLELS)));

    /* The fully diminished chord with the same steps has none. */
    HarmonyTonicProlongation diminished = example_vii65(HARMONY_MODE_MAJOR);
    assert(holds(PRO(NO_PARALLELS), &diminished));
}

static void test_other_parallels_are_still_found_in_leading_tone_chords(void) {
    /* Octaves, as ever: the tenor and bass an octave apart, both going down a step into vii7. */
    HarmonyTonicProlongation octaves = example_vii7(HARMONY_MODE_MAJOR);
    octaves.steps[1][VOICE_PART_TENOR] = -1;
    assert(!holds(PRO(NO_PARALLELS), &octaves));
    /* A perfect fifth that is not re with the sixth degree is perfect in the fully diminished
       chord too: the alto (re) and tenor (sol) a fifth apart, both going down a step. */
    HarmonyTonicProlongation fifths = example_vii7(HARMONY_MODE_MAJOR);
    fifths.steps[1][VOICE_PART_TENOR] = -3;
    fifths.steps[2][VOICE_PART_TENOR] = -4;
    assert(!holds(PRO(NO_PARALLELS), &fifths));
}

static void test_the_fully_diminished_chord_is_usual(void) {
    /* The fully diminished chord, and the triad, break no preference. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyTonicProlongation examples[] = {example_vii7(m), example_vii6_triad(m), example_vii65(m),
                                               example_vii43(m)};
        for (size_t i = 0; i < COUNT_OF(examples); i++) {
            assert(holds(PRO(AVOID_HALF_DIMINISHED), &examples[i]));
            assert(holds(PRO(RE_ABOVE_LA), &examples[i]));
        }
    }
    /* The V triad is the one the avoid-V6 rule is about, not the leading-tone triad. */
    HarmonyTonicProlongation triad = example_vii6_triad(HARMONY_MODE_MAJOR);
    assert(holds(PRO(AVOID_V6_TRIAD), &triad));
    unsigned hard = 99, soft = 99;
    assert(harmony_prolongation_check(&triad, &hard, &soft));
    assert(hard == 0 && soft == 0);
}

static void test_the_half_diminished_chord_is_less_common(void) {
    /* With re above la there are no parallel fifths, but it is the rarer chord. */
    HarmonyTonicProlongation half = example_half_diminished_7();
    assert(holds(PRO(PROGRESSION), &half));
    assert(holds(PRO(NO_PARALLELS), &half));
    assert(holds(PRO(RE_ABOVE_LA), &half));
    assert(!holds(PRO(AVOID_HALF_DIMINISHED), &half));
    unsigned hard = 99, soft = 0;
    assert(harmony_prolongation_check(&half, &hard, &soft));
    assert(hard == 0 && soft == (1u << PRO(AVOID_HALF_DIMINISHED)));
}

static void test_re_must_be_above_la_in_the_half_diminished_chord(void) {
    /* vii6/5 has re in the bass, so la is always above it: the rule is always broken, and
       so are the parallel fifths in this voicing. */
    HarmonyTonicProlongation half = example_half_diminished_65();
    assert(!holds(PRO(RE_ABOVE_LA), &half));
    assert(!holds(PRO(AVOID_HALF_DIMINISHED), &half));
    unsigned hard = 0, soft = 0;
    assert(harmony_prolongation_check(&half, &hard, &soft));
    assert(hard == (1u << PRO(NO_PARALLELS)));
    assert(soft == ((1u << PRO(AVOID_HALF_DIMINISHED)) | (1u << PRO(RE_ABOVE_LA))));

    /* With la above re in the half-diminished vii7 the rule is broken too. */
    HarmonyTonicProlongation swapped = example_half_diminished_7();
    swapped.steps[1][VOICE_PART_SOPRANO] = 12; /* la, above the re */
    swapped.steps[1][VOICE_PART_ALTO] = 8;     /* re */
    assert(!holds(PRO(RE_ABOVE_LA), &swapped));
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
    test_the_worked_progressions_resolve();
    test_ti_resolves_up_to_do();
    test_the_seventh_resolves_down_to_mi();
    test_re_goes_a_step();
    test_the_exception_for_parallel_tenths();
    test_minor_needs_the_leading_tone();
    test_no_parallel_fifths_or_octaves();
    test_leading_tone_chords_structure_and_resolution();
    test_leading_tone_chord_notes();
    test_le_falls_to_sol();
    test_minor_leading_tone_chord_needs_ti();
    test_re_and_le_are_a_diminished_fifth();
    test_half_diminished_has_perfect_fifth();
    test_other_parallels_are_still_found_in_leading_tone_chords();
    test_the_fully_diminished_chord_is_usual();
    test_the_half_diminished_chord_is_less_common();
    test_re_must_be_above_la_in_the_half_diminished_chord();
    test_the_usual_progressions_break_no_preference();
    test_v6_triad_is_less_common();
    test_v43_to_root_tonic_is_less_common();
    test_the_v_is_on_a_weaker_beat();
    test_invalid_prolongation();
    printf("test_harmony_prolongation_rules: ok\n");
    return 0;
}
