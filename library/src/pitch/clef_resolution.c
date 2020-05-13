#include "pitch/clef_resolution.h"

#define LETTERS_PER_OCTAVE 7

/* Diatonic index within an octave, C = 0 ... B = 6. PitchLetter itself is
   ordered A..G, so it cannot be used directly for stepping. */
static int letter_index(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_C:
            return 0;
        case PITCH_LETTER_D:
            return 1;
        case PITCH_LETTER_E:
            return 2;
        case PITCH_LETTER_F:
            return 3;
        case PITCH_LETTER_G:
            return 4;
        case PITCH_LETTER_A:
            return 5;
        case PITCH_LETTER_B:
        default:
            return 6;
    }
}

static PitchLetter index_letter(int index) {
    static const PitchLetter LETTERS[LETTERS_PER_OCTAVE] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E,
                                                             PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_A,
                                                             PITCH_LETTER_B};
    return LETTERS[index];
}

/* Absolute diatonic number: octave * 7 + letter index. */
static int diatonic_number(PitchLetter letter, int octave) {
    return octave * LETTERS_PER_OCTAVE + letter_index(letter);
}

int clef_reference_steps(const Clef *clef) {
    return 2 * (clef_line(clef) - 1);
}

void clef_pitch_at_position(const Clef *clef, const StaffPosition *position, PitchLetter *out_letter,
                            int *out_octave) {
    int number = diatonic_number(clef_reference_letter(clef), clef_reference_octave(clef)) +
                 (staff_position_steps(position) - clef_reference_steps(clef));

    /* Floor division/modulo so negative octaves resolve correctly. */
    int octave = number / LETTERS_PER_OCTAVE;
    int index = number % LETTERS_PER_OCTAVE;
    if (index < 0) {
        index += LETTERS_PER_OCTAVE;
        octave -= 1;
    }

    *out_letter = index_letter(index);
    *out_octave = octave;
}

StaffPositionResult clef_position_for_pitch(const Clef *clef, PitchLetter letter, int octave,
                                            StaffPosition **out) {
    int steps = clef_reference_steps(clef) +
                (diatonic_number(letter, octave) -
                 diatonic_number(clef_reference_letter(clef), clef_reference_octave(clef)));
    return staff_position_create(steps, out);
}
