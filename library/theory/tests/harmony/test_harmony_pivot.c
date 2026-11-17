#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/harmony/harmony_pivot.h"
#include "theory/tests/harmony/harmony_test_util.h"

#define MAJ HARMONY_MODE_MAJOR
#define MIN HARMONY_MODE_MINOR

static HarmonyRelatedKey key(int degree, HarmonyMode mode) {
    HarmonyRelatedKey k = {degree, mode};
    return k;
}

static bool pivots_are(HarmonyMode home, HarmonyRelatedKey k, const int expected[][2], int count) {
    HarmonyPivot found[HARMONY_PIVOT_MAX];
    if (harmony_pivot_find(home, &k, found) != count) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (found[i].old_degree != expected[i][0] || found[i].new_degree != expected[i][1]) {
            return false;
        }
    }
    return true;
}

static void test_c_major_to_g_major(void) {
    const int expected[][2] = {{1, 4}, {3, 6}, {5, 1}, {6, 2}};
    assert(pivots_are(MAJ, key(5, MAJ), expected, 4));
}

static void test_c_major_to_a_minor_shares_every_triad(void) {
    const int expected[][2] = {{1, 3}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {6, 1}, {7, 2}};
    assert(pivots_are(MAJ, key(6, MIN), expected, 7));
}

static void test_c_major_to_d_minor(void) {
    const int expected[][2] = {{1, 7}, {2, 1}, {4, 3}, {6, 5}};
    assert(pivots_are(MAJ, key(2, MIN), expected, 4));
}

static void test_d_minor_to_f_major(void) {
    const int expected[][2] = {{1, 6}, {2, 7}, {3, 1}, {4, 2}, {5, 3}, {6, 4}, {7, 5}};
    assert(pivots_are(MIN, key(3, MAJ), expected, 7));
}

static void test_c_minor_to_g_minor(void) {
    const int expected[][2] = {{1, 4}, {3, 6}, {5, 1}, {7, 3}};
    assert(pivots_are(MIN, key(5, MIN), expected, 4));
}

static void test_every_closely_related_key_has_a_pivot(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        for (int i = 0; i < harmony_related_key_count((HarmonyMode)mode); i++) {
            HarmonyRelatedKey k;
            HarmonyPivot found[HARMONY_PIVOT_MAX];
            assert(harmony_related_key_at((HarmonyMode)mode, i, &k));
            int count = harmony_pivot_find((HarmonyMode)mode, &k, found);
            assert(count >= 1 && count <= HARMONY_PIVOT_MAX);
            for (int j = 0; j < count; j++) {
                assert(harmony_pivot_valid((HarmonyMode)mode, &k, &found[j]));
            }
        }
    }
}

static void test_no_pivots_for_a_key_that_is_not_closely_related(void) {
    HarmonyPivot found[HARMONY_PIVOT_MAX];
    HarmonyRelatedKey tonic = key(1, MAJ);
    HarmonyRelatedKey wrong_mode = key(5, MIN);
    assert(harmony_pivot_find(MAJ, &tonic, found) == 0);
    assert(harmony_pivot_find(MAJ, &wrong_mode, found) == 0);
    assert(harmony_pivot_find(MAJ, NULL, found) == 0);
    assert(harmony_pivot_find(HARMONY_MODE_COUNT, &tonic, found) == 0);
    HarmonyRelatedKey v = key(5, MAJ);
    assert(harmony_pivot_find(MAJ, &v, NULL) == 0);
}

static void test_a_pair_that_is_not_a_pivot(void) {
    HarmonyRelatedKey v = key(5, MAJ);
    HarmonyPivot good = {1, 4};
    HarmonyPivot bad = {1, 5};
    assert(harmony_pivot_valid(MAJ, &v, &good));
    assert(!harmony_pivot_valid(MAJ, &v, &bad));
    assert(!harmony_pivot_valid(MAJ, &v, NULL));
}

static bool rank_is(HarmonyMode home, HarmonyRelatedKey k, HarmonyPivot pivot, HarmonyPivotRank expected) {
    HarmonyPivotRank rank = HARMONY_PIVOT_RANK_COUNT;
    return harmony_pivot_rank(home, &k, &pivot, &rank) && rank == expected;
}

static void test_the_ranks_of_the_pivots(void) {
    /* Predominant in both keys is best: iv becomes ii, and the weak ones, iii becomes vi. */
    assert(rank_is(MIN, key(3, MAJ), (HarmonyPivot){4, 2}, HARMONY_PIVOT_BEST));
    assert(rank_is(MAJ, key(5, MAJ), (HarmonyPivot){6, 2}, HARMONY_PIVOT_BEST));
    assert(rank_is(MAJ, key(5, MAJ), (HarmonyPivot){3, 6}, HARMONY_PIVOT_BEST));
    /* A tonic that becomes a predominant is second best: I becomes IV. */
    assert(rank_is(MAJ, key(5, MAJ), (HarmonyPivot){1, 4}, HARMONY_PIVOT_GOOD));
    /* A dominant is poor: V becomes I. */
    assert(rank_is(MAJ, key(5, MAJ), (HarmonyPivot){5, 1}, HARMONY_PIVOT_POOR));
    /* A predominant that becomes a tonic is neither: IV becomes I in C major to F major. */
    assert(rank_is(MAJ, key(4, MAJ), (HarmonyPivot){4, 1}, HARMONY_PIVOT_FAIR));
}

static void test_the_rank_names_and_invalid_input(void) {
    HarmonyRelatedKey v = key(5, MAJ);
    HarmonyPivot good = {1, 4}, bad = {1, 5};
    HarmonyPivotRank rank = HARMONY_PIVOT_RANK_COUNT;
    assert(!harmony_pivot_rank(MAJ, &v, &bad, &rank) && rank == HARMONY_PIVOT_RANK_COUNT);
    assert(!harmony_pivot_rank(MAJ, &v, NULL, &rank));
    assert(!harmony_pivot_rank(MAJ, &v, &good, NULL));
}

int main(void) {
    test_c_major_to_g_major();
    test_c_major_to_a_minor_shares_every_triad();
    test_c_major_to_d_minor();
    test_d_minor_to_f_major();
    test_c_minor_to_g_minor();
    test_every_closely_related_key_has_a_pivot();
    test_no_pivots_for_a_key_that_is_not_closely_related();
    test_a_pair_that_is_not_a_pivot();
    test_the_ranks_of_the_pivots();
    test_the_rank_names_and_invalid_input();
    printf("test_harmony_pivot: all tests passed\n");
    return 0;
}
