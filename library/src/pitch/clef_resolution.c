#include "pitch/clef_resolution.h"

#include "pitch/pitch_arithmetic.h"

#define LETTERS_PER_OCTAVE 7

/* Absolute diatonic number: octave * 7 + letter index. */
static int diatonic_number(PitchLetter letter, int octave) {
    return octave * LETTERS_PER_OCTAVE + pitch_letter_index(letter);
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

    *out_letter = pitch_letter_offset(PITCH_LETTER_C, index);
    *out_octave = octave;
    return CLEF_RESOLUTION_OK;
}

ClefResolutionResult clef_sounding_pitch_at_position(const Clef *clef, const StaffPosition *position,
                                                     PitchLetter *out_letter, int *out_octave) {
    PitchLetter letter;
    int octave;
    ClefResolutionResult result = clef_pitch_at_position(clef, position, &letter, &octave);
    if (result != CLEF_RESOLUTION_OK) {
        return result;
    }
    *out_letter = letter;
    *out_octave = octave + clef_octave_shift(clef);
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
