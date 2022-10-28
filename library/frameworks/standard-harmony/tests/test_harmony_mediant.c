#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_mediant.h"
#include "harmony_test_util.h"

static bool of(HarmonyMode mode, HarmonyChord a, HarmonyChord b, HarmonyChord c, HarmonyMediantKind *kind) {
    HarmonyChord chords[3] = {a, b, c};
    return harmony_mediant_of(mode, chords, kind);
}

static void test_kinds(void) {
    assert(harmony_mediant_kind_valid(HARMONY_MEDIANT_TO_PREDOMINANT));
    assert(harmony_mediant_kind_valid(HARMONY_MEDIANT_TO_DOMINANT));
    assert(!harmony_mediant_kind_valid(HARMONY_MEDIANT_KIND_COUNT));
    assert(!harmony_mediant_kind_valid((HarmonyMediantKind)-1));
    assert(strcmp(harmony_mediant_kind_name(HARMONY_MEDIANT_TO_PREDOMINANT), "to-predominant") == 0);
    assert(strcmp(harmony_mediant_kind_name(HARMONY_MEDIANT_TO_DOMINANT), "to-dominant") == 0);
    assert(harmony_mediant_kind_name(HARMONY_MEDIANT_KIND_COUNT) == NULL);
}

static void test_through_a_strong_predominant(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyChord next[] = {CH(4), CHI(2, 1), CH(2)};
        for (size_t i = 0; i < COUNT_OF(next); i++) {
            HarmonyMediantKind kind = HARMONY_MEDIANT_KIND_COUNT;
            assert(of(m, CH(1), CH(3), next[i], &kind) && kind == HARMONY_MEDIANT_TO_PREDOMINANT);
        }
    }
}

static void test_directly_to_v(void) {
    HarmonyMediantKind kind = HARMONY_MEDIANT_KIND_COUNT;
    assert(of(HARMONY_MODE_MAJOR, CH(1), CH(3), CH(5), &kind) && kind == HARMONY_MEDIANT_TO_DOMINANT);
    kind = HARMONY_MEDIANT_KIND_COUNT;
    assert(of(HARMONY_MODE_MINOR, CH(1), CH(3), CH(5), &kind) && kind == HARMONY_MEDIANT_TO_DOMINANT);
}

static void test_iii_may_be_in_first_inversion(void) {
    HarmonyMediantKind kind = HARMONY_MEDIANT_KIND_COUNT;
    assert(of(HARMONY_MODE_MAJOR, CH(1), CHI(3, 1), CH(4), &kind) && kind == HARMONY_MEDIANT_TO_PREDOMINANT);
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CHI(3, 2), CH(4), &kind));
}

static void test_iii_is_not_a_substitute_for_i6_and_follows_i(void) {
    HarmonyMediantKind kind = HARMONY_MEDIANT_KIND_COUNT;
    assert(!of(HARMONY_MODE_MAJOR, CHI(1, 1), CH(3), CH(4), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CHI(1, 2), CH(3), CH(4), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(5), CH(3), CH(4), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH7(5), CH(3), CH(4), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(6), CH(4), &kind));
}

static void test_other_chords_after_iii(void) {
    HarmonyMediantKind kind = HARMONY_MEDIANT_KIND_COUNT;
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(3), CHI(4, 1), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(3), CHI(5, 1), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(3), CH7(5), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(3), CH(6), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(3), CH(1), &kind));
    assert(!of(HARMONY_MODE_MAJOR, CH(1), CH(3), CADENTIAL_CHORD, &kind));
    assert(kind == HARMONY_MEDIANT_KIND_COUNT);
}

static void test_invalid_subjects(void) {
    HarmonyChord good[3] = {CH(1), CH(3), CH(4)};
    HarmonyChord bad[3] = {CH(1), CH(3), CH(9)};
    HarmonyMediantKind kind = HARMONY_MEDIANT_KIND_COUNT;
    assert(!harmony_mediant_of(HARMONY_MODE_MAJOR, NULL, &kind));
    assert(!harmony_mediant_of(HARMONY_MODE_MAJOR, good, NULL));
    assert(!harmony_mediant_of(HARMONY_MODE_COUNT, good, &kind));
    assert(!harmony_mediant_of(HARMONY_MODE_MAJOR, bad, &kind));
}

int main(void) {
    test_kinds();
    test_through_a_strong_predominant();
    test_directly_to_v();
    test_iii_may_be_in_first_inversion();
    test_iii_is_not_a_substitute_for_i6_and_follows_i();
    test_other_chords_after_iii();
    test_invalid_subjects();
    printf("test_harmony_mediant: all tests passed\n");
    return 0;
}
