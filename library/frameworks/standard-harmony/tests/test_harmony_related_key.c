#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_related_key.h"
#include "harmony_test_util.h"

#define MAJ HARMONY_MODE_MAJOR
#define MIN HARMONY_MODE_MINOR
#define COMMON HARMONY_KEY_RELATION_COMMON
#define OTHER HARMONY_KEY_RELATION_OTHER
#define NONE HARMONY_KEY_RELATION_COUNT

static HarmonyRelatedKey key(int degree, HarmonyMode mode) {
    HarmonyRelatedKey k = {degree, mode};
    return k;
}

static void test_the_numerals(void) {
    const char *const major[] = {"I", "ii", "iii", "IV", "V", "vi", "viio"};
    const char *const minor[] = {"i", "iio", "III", "iv", "v", "VI", "VII"};
    for (int degree = 1; degree <= 7; degree++) {
        assert(strcmp(harmony_triad_numeral(MAJ, degree), major[degree - 1]) == 0);
        assert(strcmp(harmony_triad_numeral(MIN, degree), minor[degree - 1]) == 0);
    }
    assert(harmony_triad_numeral(MAJ, 0) == NULL);
    assert(harmony_triad_numeral(MAJ, 8) == NULL);
    assert(harmony_triad_numeral(HARMONY_MODE_COUNT, 1) == NULL);
}

static void test_there_are_five_closely_related_keys(void) {
    assert(harmony_related_key_count(MAJ) == 5);
    assert(harmony_related_key_count(MIN) == 5);
    assert(harmony_related_key_count(HARMONY_MODE_COUNT) == 0);
}

static void test_the_keys_in_scale_order(void) {
    const HarmonyRelatedKey major[] = {{2, MIN}, {3, MIN}, {4, MAJ}, {5, MAJ}, {6, MIN}};
    const HarmonyRelatedKey minor[] = {{3, MAJ}, {4, MIN}, {5, MIN}, {6, MAJ}, {7, MAJ}};
    for (int i = 0; i < 5; i++) {
        HarmonyRelatedKey found;
        assert(harmony_related_key_at(MAJ, i, &found));
        assert(found.degree == major[i].degree && found.mode == major[i].mode);
        assert(harmony_related_key_at(MIN, i, &found));
        assert(found.degree == minor[i].degree && found.mode == minor[i].mode);
    }
    HarmonyRelatedKey untouched = {99, MAJ};
    assert(!harmony_related_key_at(MAJ, 5, &untouched));
    assert(!harmony_related_key_at(MAJ, -1, &untouched));
    assert(!harmony_related_key_at(HARMONY_MODE_COUNT, 0, &untouched));
    assert(untouched.degree == 99);
    assert(!harmony_related_key_at(MAJ, 0, NULL));
}

static void test_the_key_of_a_diatonic_chord(void) {
    HarmonyRelatedKey found;
    for (int degree = 1; degree <= 7; degree++) {
        assert(harmony_related_key_of(MAJ, degree, &found) == (degree >= 2 && degree <= 6));
        assert(harmony_related_key_of(MIN, degree, &found) == (degree >= 3 && degree <= 7));
    }
    assert(harmony_related_key_of(MIN, 5, &found) && found.mode == MIN); /* the minor key of v */
    assert(harmony_related_key_of(MAJ, 5, &found) && found.mode == MAJ);
    assert(!harmony_related_key_of(MAJ, 5, NULL));
    assert(!harmony_related_key_of(HARMONY_MODE_COUNT, 5, &found));
}

static void test_a_closely_related_key(void) {
    HarmonyRelatedKey v = key(5, MAJ), minor_v = key(5, MIN), tonic = key(1, MAJ), vii = key(7, MAJ);
    assert(harmony_key_is_closely_related(MAJ, &v));
    assert(!harmony_key_is_closely_related(MAJ, &minor_v)); /* the wrong mode */
    assert(harmony_key_is_closely_related(MIN, &minor_v));
    assert(!harmony_key_is_closely_related(MAJ, &tonic));
    assert(!harmony_key_is_closely_related(MAJ, &vii));
    assert(!harmony_key_is_closely_related(MAJ, NULL));
}

static void test_the_tonic_of_each_key(void) {
    const HarmonyRelatedKey major[] = {{2, MIN}, {3, MIN}, {4, MAJ}, {5, MAJ}, {6, MIN}};
    const int major_semitones[] = {2, 4, 5, 7, 9};
    const HarmonyRelatedKey minor[] = {{3, MAJ}, {4, MIN}, {5, MIN}, {6, MAJ}, {7, MAJ}};
    const int minor_semitones[] = {3, 5, 7, 8, 10};
    for (int i = 0; i < 5; i++) {
        assert(harmony_related_key_semitone(MAJ, &major[i]) == major_semitones[i]);
        assert(harmony_related_key_semitone(MIN, &minor[i]) == minor_semitones[i]);
    }
    HarmonyRelatedKey tonic = key(1, MAJ);
    assert(harmony_related_key_semitone(MAJ, &tonic) == -1);
    assert(harmony_related_key_semitone(MAJ, NULL) == -1);
}

static void test_the_circle_of_fifths_neighbors_of_f_major(void) {
    /* F major: Bb (IV), C (V), and the relatives Gm (ii), Dm (vi), Am (iii): a fifth either side
       of the home key and the relative of each. */
    HarmonyRelatedKey found;
    assert(harmony_related_key_of(MAJ, 4, &found) && found.mode == MAJ);
    assert(harmony_related_key_of(MAJ, 5, &found) && found.mode == MAJ);
    assert(harmony_related_key_of(MAJ, 2, &found) && found.mode == MIN);
    assert(harmony_related_key_of(MAJ, 6, &found) && found.mode == MIN);
    assert(harmony_related_key_of(MAJ, 3, &found) && found.mode == MIN);
}

static void test_the_ranks(void) {
    assert(harmony_key_relation_rank_valid(COMMON));
    assert(harmony_key_relation_rank_valid(OTHER));
    assert(!harmony_key_relation_rank_valid(NONE));
    assert(!harmony_key_relation_rank_valid((HarmonyKeyRelationRank)-1));
    assert(strcmp(harmony_key_relation_rank_name(COMMON), "common") == 0);
    assert(strcmp(harmony_key_relation_rank_name(OTHER), "other") == 0);
    assert(harmony_key_relation_rank_name(NONE) == NULL);
}

static void test_the_common_modulations(void) {
    for (int degree = 2; degree <= 6; degree++) {
        HarmonyRelatedKey k;
        assert(harmony_related_key_of(MAJ, degree, &k));
        assert(harmony_key_relation_rank(MAJ, &k) == (degree == 5 || degree == 6 ? COMMON : OTHER));
    }
    for (int degree = 3; degree <= 7; degree++) {
        HarmonyRelatedKey k;
        assert(harmony_related_key_of(MIN, degree, &k));
        assert(harmony_key_relation_rank(MIN, &k) == (degree == 3 || degree == 5 ? COMMON : OTHER));
    }
    HarmonyRelatedKey tonic = key(1, MAJ);
    assert(harmony_key_relation_rank(MAJ, &tonic) == NONE);
    assert(harmony_key_relation_rank(MAJ, NULL) == NONE);
}

int main(void) {
    test_the_numerals();
    test_there_are_five_closely_related_keys();
    test_the_keys_in_scale_order();
    test_the_key_of_a_diatonic_chord();
    test_a_closely_related_key();
    test_the_tonic_of_each_key();
    test_the_circle_of_fifths_neighbors_of_f_major();
    test_the_ranks();
    test_the_common_modulations();
    printf("test_harmony_related_key: all tests passed\n");
    return 0;
}
