#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_deceptive.h"
#include "harmony_test_util.h"

static bool of(HarmonyChord a, HarmonyChord b, HarmonyDeceptiveKind *kind) {
    HarmonyChord chords[2] = {a, b};
    return harmony_deceptive_of(chords, kind);
}

static void test_kinds(void) {
    assert(harmony_deceptive_kind_valid(HARMONY_DECEPTIVE_VI));
    assert(harmony_deceptive_kind_valid(HARMONY_DECEPTIVE_IV6));
    assert(!harmony_deceptive_kind_valid(HARMONY_DECEPTIVE_KIND_COUNT));
    assert(!harmony_deceptive_kind_valid((HarmonyDeceptiveKind)-1));
    assert(strcmp(harmony_deceptive_kind_name(HARMONY_DECEPTIVE_VI), "vi") == 0);
    assert(strcmp(harmony_deceptive_kind_name(HARMONY_DECEPTIVE_IV6), "iv6") == 0);
    assert(harmony_deceptive_kind_name(HARMONY_DECEPTIVE_KIND_COUNT) == NULL);
}

static void test_v_or_v7_to_vi(void) {
    HarmonyDeceptiveKind kind = HARMONY_DECEPTIVE_KIND_COUNT;
    assert(of(CH(5), CH(6), &kind) && kind == HARMONY_DECEPTIVE_VI);
    kind = HARMONY_DECEPTIVE_KIND_COUNT;
    assert(of(CH7(5), CH(6), &kind) && kind == HARMONY_DECEPTIVE_VI);
}

static void test_v_or_v7_to_iv6(void) {
    HarmonyDeceptiveKind kind = HARMONY_DECEPTIVE_KIND_COUNT;
    assert(of(CH(5), CHI(4, 1), &kind) && kind == HARMONY_DECEPTIVE_IV6);
    kind = HARMONY_DECEPTIVE_KIND_COUNT;
    assert(of(CH7(5), CHI(4, 1), &kind) && kind == HARMONY_DECEPTIVE_IV6);
}

static void test_other_pairs_are_not_deceptive(void) {
    HarmonyDeceptiveKind kind = HARMONY_DECEPTIVE_KIND_COUNT;
    assert(!of(CH(5), CH(1), &kind));
    assert(!of(CHI(5, 1), CH(6), &kind));
    assert(!of(CH(5), CHI(6, 1), &kind));
    assert(!of(CH(5), CH7(6), &kind));
    assert(!of(CH(5), CH(4), &kind));
    assert(!of(CH(5), CHI(4, 2), &kind));
    assert(!of(CH(1), CH(6), &kind));
    assert(!of(CH(5), CADENTIAL_CHORD, &kind));
    assert(kind == HARMONY_DECEPTIVE_KIND_COUNT);
}

static void test_invalid_subjects(void) {
    HarmonyChord good[2] = {CH(5), CH(6)};
    HarmonyChord bad[2] = {CH(5), CH(9)};
    HarmonyDeceptiveKind kind = HARMONY_DECEPTIVE_KIND_COUNT;
    assert(!harmony_deceptive_of(NULL, &kind));
    assert(!harmony_deceptive_of(good, NULL));
    assert(!harmony_deceptive_of(bad, &kind));
}

int main(void) {
    test_kinds();
    test_v_or_v7_to_vi();
    test_v_or_v7_to_iv6();
    test_other_pairs_are_not_deceptive();
    test_invalid_subjects();
    printf("test_harmony_deceptive: all tests passed\n");
    return 0;
}
