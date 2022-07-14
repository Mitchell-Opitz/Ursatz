#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_functions.h"
#include "harmony_test_util.h"

#define T HARMONIC_FUNCTION_TONIC
#define PD HARMONIC_FUNCTION_PREDOMINANT
#define D HARMONIC_FUNCTION_DOMINANT

static HarmonicFunction function_of(HarmonyMode mode, HarmonyChord chord) {
    HarmonicFunction function = (HarmonicFunction)-1;
    assert(harmony_function_of(mode, &chord, &function));
    return function;
}

static void test_mode_and_chord_validity(void) {
    assert(harmony_mode_valid(HARMONY_MODE_MAJOR));
    assert(harmony_mode_valid(HARMONY_MODE_MINOR));
    assert(!harmony_mode_valid(HARMONY_MODE_COUNT));
    assert(strcmp(harmony_mode_name(HARMONY_MODE_MAJOR), "major") == 0);
    assert(strcmp(harmony_mode_name(HARMONY_MODE_MINOR), "minor") == 0);
    assert(harmony_mode_name(HARMONY_MODE_COUNT) == NULL);

    HarmonyChord ok = CH(5);
    HarmonyChord low = CH(0);
    HarmonyChord high = CH(8);
    HarmonyChord inverted_too_far = CHI(5, 4);
    HarmonyChord negative_inversion = CHI(5, -1);
    HarmonyChord third_inversion_triad = CHI(5, 3);
    HarmonyChord leading_on_fifth = {5, 0, true, false};
    assert(harmony_chord_valid(&ok));
    assert(harmony_chord_valid(&(HarmonyChord){7, 2, true, false}));
    assert(!harmony_chord_valid(&low));
    assert(!harmony_chord_valid(&high));
    assert(!harmony_chord_valid(&inverted_too_far));
    assert(!harmony_chord_valid(&negative_inversion));
    assert(!harmony_chord_valid(&third_inversion_triad));
    assert(!harmony_chord_valid(&leading_on_fifth));
    assert(!harmony_chord_valid(NULL));
}

static void test_major_functions(void) {
    const HarmonicFunction expected[7] = {T, PD, PD, PD, D, PD, D};
    for (int degree = 1; degree <= 7; degree++) {
        assert(function_of(HARMONY_MODE_MAJOR, CH(degree)) == expected[degree - 1]);
    }
}

static void test_minor_functions(void) {
    const HarmonicFunction expected[7] = {T, PD, PD, PD, D, PD, PD};
    for (int degree = 1; degree <= 7; degree++) {
        assert(function_of(HARMONY_MODE_MINOR, CH(degree)) == expected[degree - 1]);
    }
}

static void test_minor_leading_tone_is_dominant(void) {
    /* VII (lowered seventh) is a weak predominant; vii (leading tone) is dominant. */
    assert(function_of(HARMONY_MODE_MINOR, CH(7)) == PD);
    assert(function_of(HARMONY_MODE_MINOR, LEADING_TONE_CHORD) == D);
    /* In major the flag is not needed: degree 7 is always the leading tone. */
    assert(function_of(HARMONY_MODE_MAJOR, CH(7)) == D);
    assert(function_of(HARMONY_MODE_MAJOR, LEADING_TONE_CHORD) == D);
}

static void test_inversion_does_not_change_function(void) {
    for (int degree = 1; degree <= 7; degree++) {
        for (int inversion = 1; inversion <= 2; inversion++) {
            assert(function_of(HARMONY_MODE_MAJOR, CHI(degree, inversion)) ==
                   function_of(HARMONY_MODE_MAJOR, CH(degree)));
        }
    }
}

static void test_seventh_flag(void) {
    /* A third inversion is a seventh chord's alone. */
    assert(harmony_chord_valid(&(HarmonyChord){5, 3, false, true}));
    HarmonyChord triad = CHI(5, 3);
    assert(!harmony_chord_valid(&triad));
    HarmonyChord v7 = CH7(5);
    assert(harmony_chord_valid(&v7));
    assert(!v7.leading_tone);

    /* A seventh does not change a chord's function, in either mode, in any
       inversion. */
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        for (int degree = 1; degree <= 7; degree++) {
            for (int inversion = 0; inversion <= 3; inversion++) {
                assert(function_of((HarmonyMode)mode, CH7I(degree, inversion)) ==
                       function_of((HarmonyMode)mode, CH(degree)));
            }
        }
    }
    assert(function_of(HARMONY_MODE_MAJOR, CH7(5)) == D);
    assert(function_of(HARMONY_MODE_MAJOR, CH7(2)) == PD);
    assert(function_of(HARMONY_MODE_MAJOR, CH7(1)) == T);
    assert(function_of(HARMONY_MODE_MINOR, (HarmonyChord){7, 0, true, true}) == D);
    assert(function_of(HARMONY_MODE_MINOR, CH7(7)) == PD);

    HarmonyPredominantStrength strength = HARMONY_PREDOMINANT_WEAK;
    HarmonyChord ii7 = CH7(2);
    assert(harmony_predominant_strength(HARMONY_MODE_MAJOR, &ii7, &strength));
    assert(strength == HARMONY_PREDOMINANT_STRONG);
}

static HarmonyPredominantStrength strength_of(HarmonyMode mode, HarmonyChord chord) {
    HarmonyPredominantStrength strength = (HarmonyPredominantStrength)-1;
    assert(harmony_predominant_strength(mode, &chord, &strength));
    return strength;
}

static void test_predominant_strength(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        assert(strength_of((HarmonyMode)mode, CH(2)) == HARMONY_PREDOMINANT_STRONG);
        assert(strength_of((HarmonyMode)mode, CH(4)) == HARMONY_PREDOMINANT_STRONG);
        assert(strength_of((HarmonyMode)mode, CH(3)) == HARMONY_PREDOMINANT_WEAK);
        assert(strength_of((HarmonyMode)mode, CH(6)) == HARMONY_PREDOMINANT_WEAK);
    }
    assert(strength_of(HARMONY_MODE_MINOR, CH(7)) == HARMONY_PREDOMINANT_WEAK);
}

static void test_strength_only_for_predominants(void) {
    HarmonyPredominantStrength strength = HARMONY_PREDOMINANT_STRONG;
    HarmonyChord tonic = CH(1);
    HarmonyChord dominant = CH(5);
    HarmonyChord leading = LEADING_TONE_CHORD;
    assert(!harmony_predominant_strength(HARMONY_MODE_MAJOR, &tonic, &strength));
    assert(!harmony_predominant_strength(HARMONY_MODE_MAJOR, &dominant, &strength));
    assert(!harmony_predominant_strength(HARMONY_MODE_MINOR, &leading, &strength));
    assert(strength == HARMONY_PREDOMINANT_STRONG);
}

static void test_invalid_inputs(void) {
    HarmonicFunction function = T;
    HarmonyChord chord = CH(1);
    HarmonyChord bad = CH(9);
    assert(!harmony_function_of(HARMONY_MODE_COUNT, &chord, &function));
    assert(!harmony_function_of(HARMONY_MODE_MAJOR, &bad, &function));
    assert(!harmony_function_of(HARMONY_MODE_MAJOR, NULL, &function));
    assert(!harmony_function_of(HARMONY_MODE_MAJOR, &chord, NULL));
    assert(function == T);
}

int main(void) {
    test_mode_and_chord_validity();
    test_major_functions();
    test_minor_functions();
    test_minor_leading_tone_is_dominant();
    test_inversion_does_not_change_function();
    test_seventh_flag();
    test_predominant_strength();
    test_strength_only_for_predominants();
    test_invalid_inputs();
    printf("test_harmony_functions: ok\n");
    return 0;
}
