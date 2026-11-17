#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/harmony/harmony_predominant_seventh.h"
#include "theory/tests/harmony/harmony_test_util.h"

#define PS(name) HARMONY_PREDOMINANT_SEVENTH_##name
#define RANK(name) HARMONY_PREDOMINANT_SEVENTH_RANK_##name

static bool kind_of(HarmonyChord chord, HarmonyPredominantSeventhKind *kind) {
    return harmony_predominant_seventh_of(&chord, kind);
}

static void test_the_inversions_of_ii7(void) {
    HarmonyPredominantSeventhKind kind = PS(COUNT);
    assert(kind_of(CH7(2), &kind) && kind == PS(II7));
    assert(kind_of(CH7I(2, 1), &kind) && kind == PS(II65));
    assert(kind_of(CH7I(2, 2), &kind) && kind == PS(II43));
    assert(kind_of(CH7I(2, 3), &kind) && kind == PS(II42));
}

static void test_iv7_vi7_and_iii7_in_any_inversion(void) {
    HarmonyPredominantSeventhKind kind = PS(COUNT);
    for (int inversion = 0; inversion <= 3; inversion++) {
        assert(kind_of(CH7I(4, inversion), &kind) && kind == PS(IV7));
        assert(kind_of(CH7I(6, inversion), &kind) && kind == PS(VI7));
        assert(kind_of(CH7I(3, inversion), &kind) && kind == PS(III7));
    }
}

static void test_other_chords_are_not_predominant_sevenths(void) {
    HarmonyPredominantSeventhKind kind = PS(COUNT);
    assert(!kind_of(CH(2), &kind));
    assert(!kind_of(CH(4), &kind));
    assert(!kind_of(CH7(5), &kind));
    assert(!kind_of(CH7(1), &kind));
    assert(!kind_of(VII7(0), &kind));
    assert(!kind_of(CH7(9), &kind));
    assert(kind == PS(COUNT));
}

static void test_invalid_subjects(void) {
    HarmonyChord chord = CH7(2);
    HarmonyPredominantSeventhKind kind = PS(COUNT);
    assert(!harmony_predominant_seventh_of(NULL, &kind));
    assert(!harmony_predominant_seventh_of(&chord, NULL));
}

int main(void) {
    test_the_inversions_of_ii7();
    test_iv7_vi7_and_iii7_in_any_inversion();
    test_other_chords_are_not_predominant_sevenths();
    test_invalid_subjects();
    printf("test_harmony_predominant_seventh: all tests passed\n");
    return 0;
}
