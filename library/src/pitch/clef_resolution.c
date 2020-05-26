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

ClefResolutionResult clef_reference_steps(const Clef *clef, int *out_steps) {
    if (!clef_is_pitched(clef)) {
        return CLEF_RESOLUTION_ERR_UNPITCHED;
    }
    *out_steps = 2 * (clef_line(clef) - 1);
    return CLEF_RESOLUTION_OK;
}

ClefResolutionResult clef_pitch_at_position(const Clef *clef, const StaffPosition *position,
                                            PitchLetter *out_letter, int *out_octave) {
    int reference_steps;
    ClefResolutionResult result = clef_reference_steps(clef, &reference_steps);
    if (result != CLEF_RESOLUTION_OK) {
        return result;
    }

    int number = diatonic_number(clef_reference_letter(clef), clef_reference_octave(clef)) +
                 (staff_position_steps(position) - reference_steps);

    /* Floor division/modulo so negative octaves resolve correctly. */
    int octave = number / LETTERS_PER_OCTAVE;
    int index = number % LETTERS_PER_OCTAVE;
    if (index < 0) {
        index += LETTERS_PER_OCTAVE;
        octave -= 1;
    }

    *out_letter = index_letter(index);
    *out_octave = octave;
    return CLEF_RESOLUTION_OK;
}

ClefResolutionResult clef_position_for_pitch(const Clef *clef, PitchLetter letter, int octave,
                                             StaffPosition **out) {
    int reference_steps;
    ClefResolutionResult result = clef_reference_steps(clef, &reference_steps);
    if (result != CLEF_RESOLUTION_OK) {
        return result;
    }

    int steps = reference_steps + (diatonic_number(letter, octave) -
                                   diatonic_number(clef_reference_letter(clef), clef_reference_octave(clef)));
    if (staff_position_create(steps, out) != STAFF_POSITION_OK) {
        return CLEF_RESOLUTION_ERR_ALLOC;
    }
    return CLEF_RESOLUTION_OK;
}
