#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_predominant_seventh.h"
#include "harmony_test_util.h"

#define PS(name) HARMONY_PREDOMINANT_SEVENTH_##name
#define RANK(name) HARMONY_PREDOMINANT_SEVENTH_RANK_##name

static bool kind_of(HarmonyChord chord, HarmonyPredominantSeventhKind *kind) {
    return harmony_predominant_seventh_of(&chord, kind);
}

static void test_kinds_and_names(void) {
    const char *const expected[] = {"ii7", "ii6/5", "ii4/3", "ii4/2", "iv7", "vi7", "iii7"};
    for (int i = 0; i < PS(COUNT); i++) {
        assert(harmony_predominant_seventh_kind_valid((HarmonyPredominantSeventhKind)i));
        assert(strcmp(harmony_predominant_seventh_kind_name((HarmonyPredominantSeventhKind)i), expected[i]) == 0);
    }
    assert(!harmony_predominant_seventh_kind_valid(PS(COUNT)));
    assert(!harmony_predominant_seventh_kind_valid((HarmonyPredominantSeventhKind)-1));
    assert(harmony_predominant_seventh_kind_name(PS(COUNT)) == NULL);
}

static void test_ranks_and_names(void) {
    const char *const expected[] = {"most", "somewhat", "less", "least"};
    for (int i = 0; i < RANK(COUNT); i++) {
        assert(harmony_predominant_seventh_rank_valid((HarmonyPredominantSeventhRank)i));
        assert(strcmp(harmony_predominant_seventh_rank_name((HarmonyPredominantSeventhRank)i), expected[i]) == 0);
    }
    assert(!harmony_predominant_seventh_rank_valid(RANK(COUNT)));
    assert(!harmony_predominant_seventh_rank_valid((HarmonyPredominantSeventhRank)-1));
    assert(harmony_predominant_seventh_rank_name(RANK(COUNT)) == NULL);
}

static void test_the_commonality_of_the_ii7_inversions(void) {
    assert(harmony_predominant_seventh_rank(PS(II65)) == RANK(MOST));
    assert(harmony_predominant_seventh_rank(PS(II7)) == RANK(SOMEWHAT));
    assert(harmony_predominant_seventh_rank(PS(II43)) == RANK(LESS));
    assert(harmony_predominant_seventh_rank(PS(II42)) == RANK(LEAST));
}

static void test_the_other_sevenths_are_less_common_than_ii7(void) {
    assert(harmony_predominant_seventh_rank(PS(IV7)) == RANK(LESS));
    assert(harmony_predominant_seventh_rank(PS(VI7)) == RANK(LESS));
    assert(harmony_predominant_seventh_rank(PS(III7)) == RANK(LEAST));
    assert(harmony_predominant_seventh_rank(PS(COUNT)) == RANK(COUNT));
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
    test_kinds_and_names();
    test_ranks_and_names();
    test_the_commonality_of_the_ii7_inversions();
    test_the_other_sevenths_are_less_common_than_ii7();
    test_the_inversions_of_ii7();
    test_iv7_vi7_and_iii7_in_any_inversion();
    test_other_chords_are_not_predominant_sevenths();
    test_invalid_subjects();
    printf("test_harmony_predominant_seventh: all tests passed\n");
    return 0;
}
