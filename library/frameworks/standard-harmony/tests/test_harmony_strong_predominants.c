#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_strong_predominants.h"
#include "harmony_test_util.h"

#define IV HARMONY_STRONG_PREDOMINANT_IV
#define II6 HARMONY_STRONG_PREDOMINANT_II6
#define II HARMONY_STRONG_PREDOMINANT_II

static bool kind_of(HarmonyMode mode, HarmonyChord chord, HarmonyStrongPredominant *kind) {
    return harmony_strong_predominant_of(mode, &chord, kind);
}

static void test_validity_and_names(void) {
    for (int i = 0; i < HARMONY_STRONG_PREDOMINANT_COUNT; i++) {
        assert(harmony_strong_predominant_valid((HarmonyStrongPredominant)i));
    }
    assert(!harmony_strong_predominant_valid(HARMONY_STRONG_PREDOMINANT_COUNT));
    assert(!harmony_strong_predominant_valid((HarmonyStrongPredominant)-1));
    assert(strcmp(harmony_strong_predominant_name(IV), "iv") == 0);
    assert(strcmp(harmony_strong_predominant_name(II6), "ii6") == 0);
    assert(strcmp(harmony_strong_predominant_name(II), "ii") == 0);
    assert(harmony_strong_predominant_name(HARMONY_STRONG_PREDOMINANT_COUNT) == NULL);
}

static void test_the_three_kinds(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyStrongPredominant kind = (HarmonyStrongPredominant)-1;
        assert(kind_of(m, CH(4), &kind) && kind == IV);
        assert(kind_of(m, CHI(2, 1), &kind) && kind == II6);
        assert(kind_of(m, CH(2), &kind) && kind == II);
    }
}

static void test_other_chords_are_not_strong_predominants(void) {
    HarmonyStrongPredominant kind = IV;
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        /* Weak predominants, tonic, and dominant. */
        assert(!kind_of(m, CH(6), &kind));
        assert(!kind_of(m, CH(3), &kind));
        assert(!kind_of(m, CH(1), &kind));
        assert(!kind_of(m, CH(5), &kind));
        /* A strong predominant in another inversion. */
        assert(!kind_of(m, CHI(4, 1), &kind));
        assert(!kind_of(m, CHI(4, 2), &kind));
        assert(!kind_of(m, CHI(2, 2), &kind));
        /* A seventh chord is not the triad. */
        assert(!kind_of(m, CH7(2), &kind));
        assert(!kind_of(m, CH7(4), &kind));
        assert(!kind_of(m, CH7I(2, 1), &kind));
    }
    assert(!kind_of(HARMONY_MODE_MINOR, CH(7), &kind));
    assert(kind == IV);
}

static void test_invalid_inputs(void) {
    HarmonyStrongPredominant kind = II;
    HarmonyChord bad = CH(9);
    HarmonyChord ok = CH(4);
    assert(!harmony_strong_predominant_of(HARMONY_MODE_COUNT, &ok, &kind));
    assert(!harmony_strong_predominant_of(HARMONY_MODE_MAJOR, &bad, &kind));
    assert(!harmony_strong_predominant_of(HARMONY_MODE_MAJOR, NULL, &kind));
    assert(!harmony_strong_predominant_of(HARMONY_MODE_MAJOR, &ok, NULL));
    assert(kind == II);
}

int main(void) {
    test_validity_and_names();
    test_the_three_kinds();
    test_other_chords_are_not_strong_predominants();
    test_invalid_inputs();
    printf("test_harmony_strong_predominants: ok\n");
    return 0;
}
