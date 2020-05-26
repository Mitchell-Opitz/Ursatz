#include "pitch/clef_resolution.h"

#include <assert.h>
#include <stddef.h>

static void expect_pitch(const Clef *clef, int steps, PitchLetter letter, int octave) {
    StaffPosition *position = NULL;
    assert(staff_position_create(steps, &position) == STAFF_POSITION_OK);

    PitchLetter got_letter;
    int got_octave;
    assert(clef_pitch_at_position(clef, position, &got_letter, &got_octave) == CLEF_RESOLUTION_OK);
    assert(got_letter == letter);
    assert(got_octave == octave);

    StaffPosition *back = NULL;
    assert(clef_position_for_pitch(clef, letter, octave, &back) == CLEF_RESOLUTION_OK);
    assert(staff_position_equals(position, back));

    staff_position_destroy(position);
    staff_position_destroy(back);
}

static void test_treble(void) {
    Clef *clef = NULL;
    assert(clef_create_treble(&clef) == CLEF_OK);
    int reference_steps = 0;
    assert(clef_reference_steps(clef, &reference_steps) == CLEF_RESOLUTION_OK);
    assert(reference_steps == 2);
    expect_pitch(clef, 0, PITCH_LETTER_E, 4); /* bottom line */
    expect_pitch(clef, 2, PITCH_LETTER_G, 4);
    expect_pitch(clef, 8, PITCH_LETTER_F, 5); /* top line */
    expect_pitch(clef, -2, PITCH_LETTER_C, 4); /* middle C, first ledger line below */
    expect_pitch(clef, 10, PITCH_LETTER_A, 5);
    clef_destroy(clef);
}

static void test_bass(void) {
    Clef *clef = NULL;
    assert(clef_create_bass(&clef) == CLEF_OK);
    expect_pitch(clef, 0, PITCH_LETTER_G, 2);
    expect_pitch(clef, 6, PITCH_LETTER_F, 3);
    expect_pitch(clef, 8, PITCH_LETTER_A, 3);
    expect_pitch(clef, 10, PITCH_LETTER_C, 4); /* middle C, first ledger line above */
    clef_destroy(clef);
}

static void test_alto_and_tenor(void) {
    Clef *alto = NULL, *tenor = NULL;
    assert(clef_create_alto(&alto) == CLEF_OK);
    assert(clef_create_tenor(&tenor) == CLEF_OK);
    expect_pitch(alto, 4, PITCH_LETTER_C, 4); /* middle line */
    expect_pitch(alto, 0, PITCH_LETTER_F, 3);
    expect_pitch(tenor, 6, PITCH_LETTER_C, 4); /* second line from top */
    expect_pitch(tenor, 0, PITCH_LETTER_D, 3);
    clef_destroy(alto);
    clef_destroy(tenor);
}

/* The same position names different pitches under different clefs. */
static void test_same_position_differs_by_clef(void) {
    Clef *treble = NULL, *bass = NULL;
    assert(clef_create_treble(&treble) == CLEF_OK);
    assert(clef_create_bass(&bass) == CLEF_OK);

    StaffPosition *position = NULL;
    assert(staff_position_create(4, &position) == STAFF_POSITION_OK);
    PitchLetter letter_a, letter_b;
    int octave_a, octave_b;
    assert(clef_pitch_at_position(treble, position, &letter_a, &octave_a) == CLEF_RESOLUTION_OK);
    assert(clef_pitch_at_position(bass, position, &letter_b, &octave_b) == CLEF_RESOLUTION_OK);
    assert(letter_a == PITCH_LETTER_B && octave_a == 4);
    assert(letter_b == PITCH_LETTER_D && octave_b == 3);

    staff_position_destroy(position);
    clef_destroy(treble);
    clef_destroy(bass);
}

/* Octave boundary: B to C wraps the octave number. */
static void test_octave_wrap(void) {
    Clef *treble = NULL;
    assert(clef_create_treble(&treble) == CLEF_OK);
    expect_pitch(treble, 4, PITCH_LETTER_B, 4);
    expect_pitch(treble, 5, PITCH_LETTER_C, 5);
    expect_pitch(treble, -1, PITCH_LETTER_D, 4);
    clef_destroy(treble);
}

int main(void) {
    test_treble();
    test_bass();
    test_alto_and_tenor();
    test_same_position_differs_by_clef();
    test_octave_wrap();
    return 0;
}
