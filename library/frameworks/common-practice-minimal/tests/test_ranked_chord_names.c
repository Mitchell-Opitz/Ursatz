#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "weighted_identification.h"

/*
 * Ranked chord name tests: the quality of every triad and seventh of the vocabulary, every inversion, a bass
 * that is no chord tone, and invalid chords.
 */

static CommonPracticeRankedChord triad_on(CommonPracticeTriad triad, int root) {
    CommonPracticeRankedChord chord = {0};
    chord.triad = triad;
    chord.root_pitch_class = root;
    return chord;
}

static CommonPracticeRankedChord seventh_on(CommonPracticeSeventh seventh, int root) {
    CommonPracticeRankedChord chord = {0};
    chord.is_seventh = true;
    chord.seventh = seventh;
    chord.root_pitch_class = root;
    return chord;
}

static void test_triad_qualities(void) {
    static const char *const EXPECTED[COMMON_PRACTICE_TRIAD_COUNT] = {
        "major", "minor", "minor", "major", "major", "minor", "diminished", "augmented", "major", "minor", "diminished"};
    for (int triad = 0; triad < COMMON_PRACTICE_TRIAD_COUNT; triad++) {
        CommonPracticeRankedChord chord = triad_on((CommonPracticeTriad)triad, 0);
        const char *name = common_practice_ranked_chord_quality_name(&chord);
        assert(name != NULL && strcmp(name, EXPECTED[triad]) == 0);
    }
}

static void test_seventh_qualities(void) {
    for (int seventh = 0; seventh < COMMON_PRACTICE_SEVENTH_COUNT; seventh++) {
        CommonPracticeRankedChord chord = seventh_on((CommonPracticeSeventh)seventh, 7);
        const char *name = common_practice_ranked_chord_quality_name(&chord);
        assert(name != NULL && strcmp(name, common_practice_seventh_label((CommonPracticeSeventh)seventh)) == 0);
    }
}

static void test_names_outside_the_vocabulary(void) {
    assert(common_practice_ranked_chord_quality_name(NULL) == NULL);
    CommonPracticeRankedChord bad_triad = triad_on(COMMON_PRACTICE_TRIAD_COUNT, 0);
    assert(common_practice_ranked_chord_quality_name(&bad_triad) == NULL);
    CommonPracticeRankedChord bad_seventh = seventh_on(COMMON_PRACTICE_SEVENTH_COUNT, 0);
    assert(common_practice_ranked_chord_quality_name(&bad_seventh) == NULL);
}

static void test_triad_inversions(void) {
    /* C major: C E G. */
    CommonPracticeRankedChord chord = triad_on(COMMON_PRACTICE_TRIAD_MAJOR, 0);
    assert(common_practice_ranked_chord_inversion(&chord, 0) == 0);
    assert(common_practice_ranked_chord_inversion(&chord, 4) == 1);
    assert(common_practice_ranked_chord_inversion(&chord, 7) == 2);
    /* A diminished triad on B: B D F. */
    CommonPracticeRankedChord diminished = triad_on(COMMON_PRACTICE_TRIAD_DIMINISHED, 11);
    assert(common_practice_ranked_chord_inversion(&diminished, 11) == 0);
    assert(common_practice_ranked_chord_inversion(&diminished, 2) == 1);
    assert(common_practice_ranked_chord_inversion(&diminished, 5) == 2);
}

static void test_seventh_inversions(void) {
    /* G7: G B D F. */
    CommonPracticeRankedChord chord = seventh_on(COMMON_PRACTICE_SEVENTH_DOMINANT7, 7);
    assert(common_practice_ranked_chord_inversion(&chord, 7) == 0);
    assert(common_practice_ranked_chord_inversion(&chord, 11) == 1);
    assert(common_practice_ranked_chord_inversion(&chord, 2) == 2);
    assert(common_practice_ranked_chord_inversion(&chord, 5) == 3);
}

static void test_bass_outside_the_chord(void) {
    CommonPracticeRankedChord chord = triad_on(COMMON_PRACTICE_TRIAD_MAJOR, 0);
    assert(common_practice_ranked_chord_inversion(&chord, 1) == -1);
    assert(common_practice_ranked_chord_inversion(&chord, -1) == -1);
    assert(common_practice_ranked_chord_inversion(&chord, 12) == -1);
    assert(common_practice_ranked_chord_inversion(NULL, 0) == -1);
    CommonPracticeRankedChord bad = triad_on(COMMON_PRACTICE_TRIAD_COUNT, 0);
    assert(common_practice_ranked_chord_inversion(&bad, 0) == -1);
}

int main(void) {
    test_triad_qualities();
    test_seventh_qualities();
    test_names_outside_the_vocabulary();
    test_triad_inversions();
    test_seventh_inversions();
    test_bass_outside_the_chord();
    printf("All ranked chord name tests passed.\n");
    return 0;
}
