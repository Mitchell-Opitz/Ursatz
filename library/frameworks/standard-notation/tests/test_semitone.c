#include <assert.h>
#include <stdio.h>

#include "semitone.h"
#include "spelling_test_util.h"

static void test_letter_bases(void) {
    assert(standard_notation_letter_semitone(PITCH_LETTER_C) == 0);
    assert(standard_notation_letter_semitone(PITCH_LETTER_D) == 2);
    assert(standard_notation_letter_semitone(PITCH_LETTER_E) == 4);
    assert(standard_notation_letter_semitone(PITCH_LETTER_F) == 5);
    assert(standard_notation_letter_semitone(PITCH_LETTER_G) == 7);
    assert(standard_notation_letter_semitone(PITCH_LETTER_A) == 9);
    assert(standard_notation_letter_semitone(PITCH_LETTER_B) == 11);
}

static void test_numbers_and_octaves(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);
    PitchSpelling *a4 = spell(PITCH_LETTER_A, 0, 4);
    PitchSpelling *c0 = spell(PITCH_LETTER_C, 0, 0);

    assert(standard_notation_semitone_number(c0) == 0);
    assert(standard_notation_semitone_number(c4) == 48);
    assert(standard_notation_semitone_distance(c4, c5) == STANDARD_NOTATION_SEMITONES_PER_OCTAVE);
    assert(standard_notation_semitone_distance(c4, a4) == 9);
    assert(standard_notation_semitone_distance(a4, c4) == -9);

    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(a4);
    pitch_spelling_destroy(c0);
}

/* The octave number belongs to the letter: B-sharp rolls into the next
   octave's C, C-flat back into the previous octave's B. */
static void test_octave_boundary(void) {
    PitchSpelling *b_sharp3 = spell(PITCH_LETTER_B, 1, 3);
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c_flat4 = spell(PITCH_LETTER_C, -1, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);

    assert(standard_notation_semitone_distance(b_sharp3, c4) == 0);
    assert(standard_notation_semitone_distance(c_flat4, b3) == 0);
    assert(!pitch_spelling_equals(b_sharp3, c4));

    pitch_spelling_destroy(b_sharp3);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c_flat4);
    pitch_spelling_destroy(b3);
}

static void test_pitch_class(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *c7 = spell(PITCH_LETTER_C, 0, 7);
    PitchSpelling *b_sharp = spell(PITCH_LETTER_B, 1, 3);
    PitchSpelling *c_flat = spell(PITCH_LETTER_C, -1, 4);
    PitchSpelling *below_zero = spell(PITCH_LETTER_A, 0, -1); /* negative semitone number */

    assert(standard_notation_pitch_class(c4) == 0);
    assert(standard_notation_pitch_class(c7) == 0);
    assert(standard_notation_pitch_class(b_sharp) == 0);
    assert(standard_notation_pitch_class(c_flat) == 11);
    assert(standard_notation_pitch_class(below_zero) == 9);

    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(c7);
    pitch_spelling_destroy(b_sharp);
    pitch_spelling_destroy(c_flat);
    pitch_spelling_destroy(below_zero);
}

/* Seven white keys and five black keys per octave. */
static void test_keys(void) {
    int white = 0, black = 0;
    for (int number = 0; number < STANDARD_NOTATION_SEMITONES_PER_OCTAVE; number++) {
        /* Spell each pitch class as a natural or a sharp of the letter below. */
        PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                  PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                  PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
        int accidentals[] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
        PitchSpelling *spelling = spell(letters[number], accidentals[number], 4);
        assert(standard_notation_pitch_class(spelling) == number);
        assert(standard_notation_is_white_key(spelling) != standard_notation_is_black_key(spelling));
        if (standard_notation_is_white_key(spelling)) {
            white++;
        } else {
            black++;
        }
        pitch_spelling_destroy(spelling);
    }
    assert(white == 7);
    assert(black == 5);

    assert(!standard_notation_is_white_key(NULL));
    assert(!standard_notation_is_black_key(NULL));

    /* Spelling does not matter, only the key: E-sharp is a white key. */
    PitchSpelling *e_sharp = spell(PITCH_LETTER_E, 1, 4);
    assert(standard_notation_is_white_key(e_sharp));
    pitch_spelling_destroy(e_sharp);
}

int main(void) {
    test_letter_bases();
    test_numbers_and_octaves();
    test_octave_boundary();
    test_pitch_class();
    test_keys();
    printf("test_semitone (standard-notation): ok\n");
    return 0;
}
