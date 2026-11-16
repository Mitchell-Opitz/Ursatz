#include <assert.h>
#include <stdio.h>

#include "seventh_rules.h"
#include "triad_rules.h"

/*
 * Quality semitone tables: every row is checked against the intervals of its quality written out as data (the third, fifth
 * and seventh of each quality measured in semitones), so the table cannot drift from them.
 */

static const int TRIAD_ROWS[STANDARD_TONALITY_TRIAD_QUALITY_COUNT][3] = {
    {0, 4, 7}, /* major */
    {0, 3, 7}, /* minor */
    {0, 3, 6}, /* diminished */
    {0, 4, 8}, /* augmented */
};

static const int SEVENTH_ROWS[STANDARD_TONALITY_SEVENTH_QUALITY_COUNT][4] = {
    {0, 4, 7, 11}, /* major-major */
    {0, 4, 7, 10}, /* major-minor */
    {0, 3, 7, 10}, /* minor-minor */
    {0, 3, 6, 10}, /* half-diminished */
    {0, 3, 6, 9},  /* fully diminished */
};

static void test_triad_rows_follow_the_intervals(void) {
    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        int out[3] = {-1, -1, -1};
        assert(standard_tonality_triad_quality_semitones((StandardTonalityTriadQuality)q, out));
        assert(out[0] == TRIAD_ROWS[q][0] && out[1] == TRIAD_ROWS[q][1] && out[2] == TRIAD_ROWS[q][2]);
    }
}

static void test_seventh_rows_follow_the_intervals(void) {
    for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
        int out[4] = {-1, -1, -1, -1};
        assert(standard_tonality_seventh_quality_semitones((StandardTonalitySeventhQuality)q, out));
        assert(out[0] == SEVENTH_ROWS[q][0] && out[1] == SEVENTH_ROWS[q][1] && out[2] == SEVENTH_ROWS[q][2] &&
               out[3] == SEVENTH_ROWS[q][3]);
    }
}

static void test_known_rows(void) {
    int triad[3];
    assert(standard_tonality_triad_quality_semitones(STANDARD_TONALITY_TRIAD_MAJOR, triad));
    assert(triad[0] == 0 && triad[1] == 4 && triad[2] == 7);
    assert(standard_tonality_triad_quality_semitones(STANDARD_TONALITY_TRIAD_AUGMENTED, triad));
    assert(triad[0] == 0 && triad[1] == 4 && triad[2] == 8);
    int seventh[4];
    assert(standard_tonality_seventh_quality_semitones(STANDARD_TONALITY_SEVENTH_MAJOR_MINOR, seventh));
    assert(seventh[0] == 0 && seventh[1] == 4 && seventh[2] == 7 && seventh[3] == 10);
    assert(standard_tonality_seventh_quality_semitones(STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED, seventh));
    assert(seventh[0] == 0 && seventh[1] == 3 && seventh[2] == 6 && seventh[3] == 9);
}

static void test_invalid_arguments(void) {
    int triad[3] = {5, 5, 5};
    int seventh[4] = {5, 5, 5, 5};
    assert(!standard_tonality_triad_quality_semitones(STANDARD_TONALITY_TRIAD_QUALITY_COUNT, triad));
    assert(!standard_tonality_triad_quality_semitones((StandardTonalityTriadQuality)-1, triad));
    assert(!standard_tonality_triad_quality_semitones(STANDARD_TONALITY_TRIAD_MAJOR, NULL));
    assert(triad[0] == 5 && triad[1] == 5 && triad[2] == 5);
    assert(!standard_tonality_seventh_quality_semitones(STANDARD_TONALITY_SEVENTH_QUALITY_COUNT, seventh));
    assert(!standard_tonality_seventh_quality_semitones((StandardTonalitySeventhQuality)-1, seventh));
    assert(!standard_tonality_seventh_quality_semitones(STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR, NULL));
    assert(seventh[0] == 5 && seventh[3] == 5);
}

int main(void) {
    test_triad_rows_follow_the_intervals();
    test_seventh_rows_follow_the_intervals();
    test_known_rows();
    test_invalid_arguments();
    printf("All quality semitone tests passed.\n");
    return 0;
}
