#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_iv6_prolongation.h"
#include "harmony_test_util.h"

static bool of(HarmonyChord a, HarmonyChord b, HarmonyChord c, HarmonyChord d, HarmonyIv6ProlongationKind *kind) {
    HarmonyChord chords[4] = {a, b, c, d};
    return harmony_iv6_prolongation_of(chords, kind);
}

static void test_kinds(void) {
    assert(harmony_iv6_prolongation_kind_valid(HARMONY_IV6_PROLONGATION_V));
    assert(harmony_iv6_prolongation_kind_valid(HARMONY_IV6_PROLONGATION_V6));
    assert(!harmony_iv6_prolongation_kind_valid(HARMONY_IV6_PROLONGATION_KIND_COUNT));
    assert(!harmony_iv6_prolongation_kind_valid((HarmonyIv6ProlongationKind)-1));
    assert(strcmp(harmony_iv6_prolongation_kind_name(HARMONY_IV6_PROLONGATION_V), "v") == 0);
    assert(strcmp(harmony_iv6_prolongation_kind_name(HARMONY_IV6_PROLONGATION_V6), "v6") == 0);
    assert(harmony_iv6_prolongation_kind_name(HARMONY_IV6_PROLONGATION_KIND_COUNT) == NULL);
}

static void test_both_progressions(void) {
    HarmonyIv6ProlongationKind kind = HARMONY_IV6_PROLONGATION_KIND_COUNT;
    assert(of(CH(1), CHI(4, 1), CH(5), CH(1), &kind) && kind == HARMONY_IV6_PROLONGATION_V);
    kind = HARMONY_IV6_PROLONGATION_KIND_COUNT;
    assert(of(CH(1), CHI(4, 1), CHI(5, 1), CH(1), &kind) && kind == HARMONY_IV6_PROLONGATION_V6);
}

static void test_other_chords_are_not_the_progression(void) {
    HarmonyIv6ProlongationKind kind = HARMONY_IV6_PROLONGATION_KIND_COUNT;
    assert(!of(CH(1), CH(4), CH(5), CH(1), &kind));
    assert(!of(CH(1), CHI(4, 1), CH7(5), CH(1), &kind));
    assert(!of(CH(1), CHI(4, 1), CHI(5, 2), CH(1), &kind));
    assert(!of(CH(1), CHI(4, 1), CH(5), CHI(1, 1), &kind));
    assert(!of(CHI(1, 1), CHI(4, 1), CH(5), CH(1), &kind));
    assert(!of(CH(1), CHI(2, 1), CH(5), CH(1), &kind));
    assert(!of(CH(1), CHI(4, 1), CH(5), CADENTIAL_CHORD, &kind));
    assert(kind == HARMONY_IV6_PROLONGATION_KIND_COUNT);
}

static void test_invalid_subjects(void) {
    HarmonyChord good[4] = {CH(1), CHI(4, 1), CH(5), CH(1)};
    HarmonyChord bad[4] = {CH(1), CHI(4, 1), CH(9), CH(1)};
    HarmonyIv6ProlongationKind kind = HARMONY_IV6_PROLONGATION_KIND_COUNT;
    assert(!harmony_iv6_prolongation_of(NULL, &kind));
    assert(!harmony_iv6_prolongation_of(good, NULL));
    assert(!harmony_iv6_prolongation_of(bad, &kind));
}

int main(void) {
    test_kinds();
    test_both_progressions();
    test_other_chords_are_not_the_progression();
    test_invalid_subjects();
    printf("test_harmony_iv6_prolongation: all tests passed\n");
    return 0;
}
