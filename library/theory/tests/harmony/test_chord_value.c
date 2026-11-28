#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/harmony/chord_value.h"

/*
 * Hindemith's chord groups (R-764): the eleven sonorities of the plan (C E G Ia, E G C Ib, G B D F IIa, B D F G IIb2, C D G IIIa, C E G B
 * IIIa, C E G Bb Db IVa, C E G# V, C F Bb V, B D F VI, B D F Ab VI) classify as named, with the rank and the root of the group; a
 * pitch class doubled counts once; fewer than three pitch classes and null arguments are refused; both tables hold ranks 1 to 12 once.
 */

static void expect(const int *pitches, size_t count, const char *group, int rank, int root, bool in_bass) {
    ChordValue value = {NULL, 0, 0, false};
    assert(chord_value_classify(pitches, count, &value));
    if (strcmp(value.group, group) != 0 || value.rank != rank || value.root_pitch_class != root || value.root_in_bass != in_bass) {
        fprintf(stderr, "expected %s rank %d root %d bass %d, got %s rank %d root %d bass %d\n", group, rank, root, in_bass, value.group,
                value.rank, value.root_pitch_class, value.root_in_bass);
    }
    assert(strcmp(value.group, group) == 0 && value.rank == rank && value.root_pitch_class == root && value.root_in_bass == in_bass);
}

static void test_groups(void) {
    const int ia[] = {60, 64, 67};
    const int ib[] = {64, 67, 72};
    const int iia[] = {55, 59, 62, 65};
    const int iib2[] = {59, 62, 65, 67};
    const int iiia[] = {60, 62, 67};
    const int iiia_seventh[] = {60, 64, 67, 71};
    const int iva[] = {60, 64, 67, 70, 73};
    const int augmented[] = {60, 64, 68};
    const int fourths[] = {60, 65, 70};
    const int diminished[] = {59, 62, 65};
    const int diminished_seventh[] = {59, 62, 65, 68};
    expect(ia, 3, "ia", 1, 0, true);
    expect(ib, 3, "ib", 2, 0, false);
    expect(iia, 4, "iia", 3, 7, true);
    expect(iib2, 4, "iib2", 5, 7, false);
    expect(iiia, 3, "iiia", 7, 0, true);
    expect(iiia_seventh, 4, "iiia", 7, 0, true);
    expect(iva, 5, "iva", 9, 0, true);
    expect(augmented, 3, "v", 11, -1, false);
    expect(fourths, 3, "v", 11, -1, false);
    expect(diminished, 3, "vi", 12, -1, false);
    expect(diminished_seventh, 4, "vi", 12, -1, false);
    /* a doubled tone changes nothing; C D G in other octaves is not two stacked fourths */
    const int doubled[] = {48, 60, 64, 67, 72};
    expect(doubled, 5, "ia", 1, 0, true);
    const int quartal[] = {62, 67, 72};
    expect(quartal, 3, "v", 11, -1, false);
}

static void test_refusals(void) {
    const int two[] = {60, 67};
    const int octave_and_fifth[] = {60, 67, 72};
    ChordValue value = {"unchanged", 7, 7, true};
    assert(!chord_value_classify(two, 2, &value) && strcmp(value.group, "unchanged") == 0 && value.rank == 7);
    assert(!chord_value_classify(octave_and_fifth, 3, &value));
    assert(!chord_value_classify(NULL, 3, &value) && !chord_value_classify(two, 0, &value) && !chord_value_classify(two, 2, NULL));
}

static void test_tables(void) {
    bool ranks[CHORD_VALUE_RANKS + 1] = {false};
    bool interval_ranks[CHORD_VALUE_RANKS + 1] = {false};
    for (int i = 0; i < CHORD_VALUE_RANKS; i++) {
        assert(CHORD_VALUE_GROUPS[i].rank == i + 1 && CHORD_VALUE_SERIES_TWO[i].rank == i + 1);
        assert(!ranks[CHORD_VALUE_GROUPS[i].rank] && !interval_ranks[CHORD_VALUE_SERIES_TWO[i].rank]);
        ranks[CHORD_VALUE_GROUPS[i].rank] = interval_ranks[CHORD_VALUE_SERIES_TWO[i].rank] = true;
        assert(CHORD_VALUE_GROUPS[i].group >= 1 && CHORD_VALUE_GROUPS[i].group <= 6);
        assert(i == 0 || CHORD_VALUE_GROUPS[i].group >= CHORD_VALUE_GROUPS[i - 1].group);
    }
    assert(strcmp(CHORD_VALUE_SERIES_TWO[1].code, "fifth") == 0 && CHORD_VALUE_SERIES_TWO[11].root == CHORD_VALUE_ROOT_NONE);
}

int main(void) {
    test_groups();
    test_refusals();
    test_tables();
    return 0;
}
