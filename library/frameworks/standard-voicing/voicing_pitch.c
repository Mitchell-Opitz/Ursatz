#include "voicing_pitch.h"

#include <stddef.h>

static int letter_semitone(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_C:
            return 0;
        case PITCH_LETTER_D:
            return 2;
        case PITCH_LETTER_E:
            return 4;
        case PITCH_LETTER_F:
            return 5;
        case PITCH_LETTER_G:
            return 7;
        case PITCH_LETTER_A:
            return 9;
        case PITCH_LETTER_B:
        default:
            return 11;
    }
}

static int letter_step(PitchLetter letter) {
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

int standard_voicing_semitone_number(const PitchSpelling *spelling) {
    return STANDARD_VOICING_SEMITONES_PER_OCTAVE * pitch_spelling_octave(spelling) +
           letter_semitone(pitch_spelling_letter(spelling)) + pitch_spelling_accidental(spelling);
}

int standard_voicing_step_number(const PitchSpelling *spelling) {
    return 7 * pitch_spelling_octave(spelling) + letter_step(pitch_spelling_letter(spelling));
}

int standard_voicing_generic_size(const PitchSpelling *a, const PitchSpelling *b) {
    int distance = standard_voicing_step_number(a) - standard_voicing_step_number(b);
    if (distance < 0) {
        distance = -distance;
    }
    return distance + 1;
}

bool standard_voicing_same_pitch_class(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return pitch_spelling_letter(a) == pitch_spelling_letter(b) &&
           pitch_spelling_accidental(a) == pitch_spelling_accidental(b);
}
