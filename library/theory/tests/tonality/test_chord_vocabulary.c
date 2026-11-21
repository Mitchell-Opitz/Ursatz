#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/chord_vocabulary.h"
#include "theory/tonality/chord_vocabulary_seventh.h"

/*
 * Construction tests: one per diatonic triad (I through vii\xC2\xB0),
 * confirming each gives a candidate labeled with its actual
 * roman numeral.
 */

static void check_triad(CommonPracticeTriad triad, const char *expected_label) {
    ChordCandidate chord;
    assert(common_practice_triad_candidate(triad, 1, 1, &chord));
    assert(strcmp(chord.label, expected_label) == 0);
    assert(chord.confidence.num == 1 && chord.confidence.den == 1);
    assert(chord.structure.root_pitch_class == -1 && chord.structure.bass_pitch_class == -1 && chord.structure.interval_count == 0);
}

/* The confidence is the given fraction, reduced; a zero denominator and an invalid triad are refused and leave the candidate alone. */
static void check_confidence_and_refusals(void) {
    ChordCandidate chord;
    memset(&chord, 0, sizeof(chord));
    assert(common_practice_triad_candidate(COMMON_PRACTICE_TRIAD_V, 500000, 1000000, &chord));
    assert(chord.confidence.num == 1 && chord.confidence.den == 2);
    assert(common_practice_seventh_candidate(COMMON_PRACTICE_SEVENTH_DOMINANT7, 250000, 1000000, &chord));
    assert(chord.confidence.num == 1 && chord.confidence.den == 4 && strcmp(chord.label, "dominant7") == 0);
    ChordCandidate untouched;
    memset(&untouched, 7, sizeof(untouched));
    ChordCandidate copy = untouched;
    assert(!common_practice_triad_candidate(COMMON_PRACTICE_TRIAD_V, 1, 0, &untouched));
    assert(!common_practice_triad_candidate(COMMON_PRACTICE_TRIAD_COUNT, 1, 1, &untouched));
    assert(!common_practice_seventh_candidate(COMMON_PRACTICE_SEVENTH_COUNT, 1, 1, &untouched));
    assert(memcmp(&untouched, &copy, sizeof(copy)) == 0);
}

static void check_triad_intervals(CommonPracticeTriad triad, const int expected[3]) {
    int got[3];
    assert(common_practice_triad_intervals(triad, got));
    assert(memcmp(got, expected, sizeof(got)) == 0);
}

/* The chord qualities as root-relative interval sets, with their names. */
static void check_quality_set(void) {
    const int major[3] = {0, 4, 7};
    const int minor[3] = {0, 3, 7};
    const int diminished[3] = {0, 3, 6};
    const int augmented[3] = {0, 4, 8};
    check_triad_intervals(COMMON_PRACTICE_TRIAD_I, major);
    check_triad_intervals(COMMON_PRACTICE_TRIAD_II, minor);
    check_triad_intervals(COMMON_PRACTICE_TRIAD_VII_DIM, diminished);
    check_triad_intervals(COMMON_PRACTICE_TRIAD_AUGMENTED, augmented);
    check_triad_intervals(COMMON_PRACTICE_TRIAD_MAJOR, major);
    check_triad_intervals(COMMON_PRACTICE_TRIAD_MINOR, minor);
    check_triad_intervals(COMMON_PRACTICE_TRIAD_DIMINISHED, diminished);
    int unused[3];
    assert(!common_practice_triad_intervals(COMMON_PRACTICE_TRIAD_COUNT, unused));
    assert(!common_practice_triad_intervals((CommonPracticeTriad)-1, unused));

    const int dominant7[4] = {0, 4, 7, 10};
    const int half_diminished7[4] = {0, 3, 6, 10};
    const int diminished7[4] = {0, 3, 6, 9};
    int got[4];
    assert(common_practice_seventh_intervals(COMMON_PRACTICE_SEVENTH_DOMINANT7, got));
    assert(memcmp(got, dominant7, sizeof(got)) == 0);
    assert(common_practice_seventh_intervals(COMMON_PRACTICE_SEVENTH_HALF_DIMINISHED7, got));
    assert(memcmp(got, half_diminished7, sizeof(got)) == 0);
    assert(common_practice_seventh_intervals(COMMON_PRACTICE_SEVENTH_DIMINISHED7, got));
    assert(memcmp(got, diminished7, sizeof(got)) == 0);
    int unused4[4];
    assert(!common_practice_seventh_intervals(COMMON_PRACTICE_SEVENTH_COUNT, unused4));

    assert(COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT == (int)COMMON_PRACTICE_TRIAD_AUGMENTED);
    assert(COMMON_PRACTICE_SEVENTH_MAJOR_KEY_COUNT == (int)COMMON_PRACTICE_SEVENTH_DIMINISHED7);
}

static void check_seventh(CommonPracticeSeventh seventh, const char *expected_label) {
    assert(strcmp(common_practice_seventh_label(seventh), expected_label) == 0);
    ChordCandidate chord;
    assert(common_practice_seventh_candidate(seventh, 1, 1, &chord));
    assert(strcmp(chord.label, expected_label) == 0);
}

static void check_symbol_suffixes(void) {
    static const char *const PAIRS[9][2] = {{"major", ""},        {"minor", "m"},          {"dominant7", "7"},
                                            {"major7", "maj7"},   {"minor7", "m7"},        {"diminished", "dim"},
                                            {"diminished7", "dim7"}, {"half-diminished7", "m7b5"}, {"augmented", "aug"}};
    for (int i = 0; i < 9; i++) {
        assert(strcmp(common_practice_chord_symbol_suffix(PAIRS[i][0]), PAIRS[i][1]) == 0);
    }
    assert(strcmp(common_practice_chord_symbol_suffix("sus4"), "sus4") == 0);
    assert(common_practice_chord_symbol_suffix(NULL) == NULL);
}

int main(void) {
    check_triad(COMMON_PRACTICE_TRIAD_I, "I");
    check_triad(COMMON_PRACTICE_TRIAD_II, "ii");
    check_triad(COMMON_PRACTICE_TRIAD_III, "iii");
    check_triad(COMMON_PRACTICE_TRIAD_IV, "IV");
    check_triad(COMMON_PRACTICE_TRIAD_V, "V");
    check_triad(COMMON_PRACTICE_TRIAD_VI, "vi");
    check_triad(COMMON_PRACTICE_TRIAD_VII_DIM, "vii\xC2\xB0");

    check_triad(COMMON_PRACTICE_TRIAD_AUGMENTED, "augmented");
    check_triad(COMMON_PRACTICE_TRIAD_MAJOR, "major");
    check_triad(COMMON_PRACTICE_TRIAD_MINOR, "minor");
    check_triad(COMMON_PRACTICE_TRIAD_DIMINISHED, "diminished");
    check_seventh(COMMON_PRACTICE_SEVENTH_DIMINISHED7, "diminished7");
    check_quality_set();
    check_confidence_and_refusals();
    check_symbol_suffixes();

    printf("test_chord_vocabulary: ok\n");
    return 0;
}
