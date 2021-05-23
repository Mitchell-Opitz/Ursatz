#include <assert.h>
#include <stdio.h>

#include "construction_rules.h"
#include "voicing_test_util.h"

typedef StandardVoicingConstruction Construction;

#define OK STANDARD_VOICING_CONSTRUCTION_OK

/* B diminished triad: B D F. */
static void b_diminished(const PitchSpelling *tones[3]) {
    tones[0] = P(B, 0, 3);
    tones[1] = P(D, 0, 4);
    tones[2] = P(F, 0, 4);
}

/* G dominant seventh: G B D F. */
static void g7(const PitchSpelling *tones[4]) {
    tones[0] = P(G, 0, 2);
    tones[1] = P(B, 0, 2);
    tones[2] = P(D, 0, 3);
    tones[3] = P(F, 0, 3);
}

/* Example 2: the corrected chord is a B diminished triad in first
   inversion; the erroneous one has accidentals on two pitches. */
static void test_b_diminished_first_inversion(void) {
    const PitchSpelling *tones[3];
    b_diminished(tones);

    StandardVoicingChord correct = make_chord(P(D, 0, 4), P(B, 0, 3), P(F, 0, 3), P(D, 0, 3));
    assert(standard_voicing_check_construction(&correct, tones, 3, 1) == OK);

    StandardVoicingChord accidentals = make_chord(P(D, 1, 4), P(B, 0, 3), P(F, 1, 3), P(D, 0, 3));
    assert(standard_voicing_check_construction(&accidentals, tones, 3, 1) ==
           STANDARD_VOICING_CONSTRUCTION_FOREIGN_NOTE);

    /* Enharmonic spellings are different notes. */
    StandardVoicingChord enharmonic = make_chord(P(E, -2, 4), P(B, 0, 3), P(F, 0, 3), P(D, 0, 3));
    assert(standard_voicing_check_construction(&enharmonic, tones, 3, 1) ==
           STANDARD_VOICING_CONSTRUCTION_FOREIGN_NOTE);
}

static void test_bass_must_match_inversion(void) {
    const PitchSpelling *tones[3];
    b_diminished(tones);

    StandardVoicingChord first_inversion = make_chord(P(D, 0, 4), P(B, 0, 3), P(F, 0, 3), P(D, 0, 3));
    assert(standard_voicing_check_construction(&first_inversion, tones, 3, 0) ==
           STANDARD_VOICING_CONSTRUCTION_BASS_MISMATCH);
    assert(standard_voicing_check_construction(&first_inversion, tones, 3, 2) ==
           STANDARD_VOICING_CONSTRUCTION_BASS_MISMATCH);

    StandardVoicingChord root_position = make_chord(P(D, 0, 4), P(F, 0, 4), P(D, 0, 3), P(B, 0, 2));
    assert(standard_voicing_check_construction(&root_position, tones, 3, 0) == OK);

    StandardVoicingChord second_inversion = make_chord(P(D, 0, 4), P(B, 0, 3), P(D, 0, 3), P(F, 0, 2));
    assert(standard_voicing_check_construction(&second_inversion, tones, 3, 2) == OK);
}

static void test_no_tone_missing(void) {
    const PitchSpelling *tones[3];
    b_diminished(tones);

    /* The fifth (F) sounds in no voice. */
    StandardVoicingChord missing = make_chord(P(D, 0, 4), P(B, 0, 3), P(D, 0, 3), P(D, 0, 3));
    assert(standard_voicing_check_construction(&missing, tones, 3, 1) == STANDARD_VOICING_CONSTRUCTION_MISSING_TONE);
}

static void test_seventh_chord(void) {
    const PitchSpelling *tones[4];
    g7(tones);

    StandardVoicingChord root = make_chord(P(B, 0, 4), P(F, 0, 4), P(D, 0, 4), P(G, 0, 2));
    assert(standard_voicing_check_construction(&root, tones, 4, 0) == OK);

    /* Third inversion: the seventh in the bass. */
    StandardVoicingChord third_inversion = make_chord(P(D, 0, 5), P(B, 0, 4), P(G, 0, 3), P(F, 0, 3));
    assert(standard_voicing_check_construction(&third_inversion, tones, 4, 3) == OK);
    assert(standard_voicing_check_construction(&third_inversion, tones, 4, 0) ==
           STANDARD_VOICING_CONSTRUCTION_BASS_MISMATCH);

    /* A seventh chord with its seventh missing. */
    StandardVoicingChord missing = make_chord(P(B, 0, 4), P(G, 0, 4), P(D, 0, 4), P(G, 0, 2));
    assert(standard_voicing_check_construction(&missing, tones, 4, 0) == STANDARD_VOICING_CONSTRUCTION_MISSING_TONE);
}

static void test_invalid_input(void) {
    const PitchSpelling *tones[3];
    b_diminished(tones);
    StandardVoicingChord chord = make_chord(P(D, 0, 4), P(B, 0, 3), P(F, 0, 3), P(D, 0, 3));

    assert(standard_voicing_check_construction(NULL, tones, 3, 1) == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);
    assert(standard_voicing_check_construction(&chord, NULL, 3, 1) == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);
    assert(standard_voicing_check_construction(&chord, tones, 2, 0) == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);
    assert(standard_voicing_check_construction(&chord, tones, 5, 0) == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);
    assert(standard_voicing_check_construction(&chord, tones, 3, -1) == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);
    /* A triad has no third inversion. */
    assert(standard_voicing_check_construction(&chord, tones, 3, 3) == STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);

    StandardVoicingChord incomplete = make_chord(P(D, 0, 4), NULL, P(F, 0, 3), P(D, 0, 3));
    assert(standard_voicing_check_construction(&incomplete, tones, 3, 1) ==
           STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);

    const PitchSpelling *with_null[3] = {P(B, 0, 3), NULL, P(F, 0, 4)};
    assert(standard_voicing_check_construction(&chord, with_null, 3, 1) ==
           STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT);
}

static void test_inversion_from_figure(void) {
    int inversion = -1;

    /* Triads. */
    const char *root[] = {"", "5", "53"};
    for (int i = 0; i < 3; i++) {
        assert(standard_voicing_inversion_from_figure(root[i], 3, &inversion) && inversion == 0);
    }
    assert(standard_voicing_inversion_from_figure("6", 3, &inversion) && inversion == 1);
    assert(standard_voicing_inversion_from_figure("63", 3, &inversion) && inversion == 1);
    assert(standard_voicing_inversion_from_figure("64", 3, &inversion) && inversion == 2);

    /* Seventh chords. */
    assert(standard_voicing_inversion_from_figure("7", 4, &inversion) && inversion == 0);
    assert(standard_voicing_inversion_from_figure("65", 4, &inversion) && inversion == 1);
    assert(standard_voicing_inversion_from_figure("43", 4, &inversion) && inversion == 2);
    assert(standard_voicing_inversion_from_figure("42", 4, &inversion) && inversion == 3);
    assert(standard_voicing_inversion_from_figure("2", 4, &inversion) && inversion == 3);

    /* A figure that does not belong to the chord size leaves the output alone. */
    inversion = -1;
    assert(!standard_voicing_inversion_from_figure("7", 3, &inversion));
    assert(!standard_voicing_inversion_from_figure("64", 4, &inversion));
    assert(!standard_voicing_inversion_from_figure("", 4, &inversion));
    assert(!standard_voicing_inversion_from_figure("9", 3, &inversion));
    assert(!standard_voicing_inversion_from_figure("6", 5, &inversion));
    assert(!standard_voicing_inversion_from_figure(NULL, 3, &inversion));
    assert(!standard_voicing_inversion_from_figure("6", 3, NULL));
    assert(inversion == -1);
}

int main(void) {
    test_b_diminished_first_inversion();
    test_bass_must_match_inversion();
    test_no_tone_missing();
    test_seventh_chord();
    test_invalid_input();
    test_inversion_from_figure();
    printf("test_construction_rules (standard-voicing): ok\n");
    return 0;
}
