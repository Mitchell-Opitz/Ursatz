#include <assert.h>
#include <stdio.h>

#include "seventh_rules.h"
#include "triad_rules.h"

/*
 * Quality semitone tables: every row is checked against the framework's own interval rules (the third, fifth and
 * seventh of each quality measured in semitones), so the table cannot drift from them.
 */

static int semitones_of(int size, StandardTonalityIntervalQuality quality) {
    int semitones = -1;
    assert(standard_tonality_interval_semitones(size, quality, &semitones));
    return semitones;
}

static void test_triad_rows_follow_the_interval_rules(void) {
    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        StandardTonalityIntervalQuality third;
        StandardTonalityIntervalQuality fifth;
        assert(standard_tonality_triad_intervals((StandardTonalityTriadQuality)q, &third, &fifth));
        int out[3] = {-1, -1, -1};
        assert(standard_tonality_triad_quality_semitones((StandardTonalityTriadQuality)q, out));
        assert(out[0] == 0);
        assert(out[1] == semitones_of(3, third));
        assert(out[2] == semitones_of(5, fifth));
    }
}

static void test_seventh_rows_follow_the_interval_rules(void) {
    for (int q = 0; q < STANDARD_TONALITY_SEVENTH_QUALITY_COUNT; q++) {
        StandardTonalityTriadQuality triad;
        StandardTonalityIntervalQuality seventh;
        assert(standard_tonality_seventh_parts((StandardTonalitySeventhQuality)q, &triad, &seventh));
        int triad_out[3];
        assert(standard_tonality_triad_quality_semitones(triad, triad_out));
        int out[4] = {-1, -1, -1, -1};
        assert(standard_tonality_seventh_quality_semitones((StandardTonalitySeventhQuality)q, out));
        assert(out[0] == triad_out[0] && out[1] == triad_out[1] && out[2] == triad_out[2]);
        assert(out[3] == semitones_of(7, seventh));
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
    test_triad_rows_follow_the_interval_rules();
    test_seventh_rows_follow_the_interval_rules();
    test_known_rows();
    test_invalid_arguments();
    printf("All quality semitone tests passed.\n");
    return 0;
}
