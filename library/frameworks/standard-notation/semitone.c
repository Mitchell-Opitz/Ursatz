#include "semitone.h"

#include <stddef.h>

int standard_notation_letter_semitone(PitchLetter letter) {
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

int standard_notation_semitone_number(const PitchSpelling *spelling) {
    return STANDARD_NOTATION_SEMITONES_PER_OCTAVE * pitch_spelling_octave(spelling) +
           standard_notation_letter_semitone(pitch_spelling_letter(spelling)) + pitch_spelling_accidental(spelling);
}

int standard_notation_semitone_distance(const PitchSpelling *from, const PitchSpelling *to) {
    return standard_notation_semitone_number(to) - standard_notation_semitone_number(from);
}

int standard_notation_pitch_class(const PitchSpelling *spelling) {
    int number = standard_notation_semitone_number(spelling) % STANDARD_NOTATION_SEMITONES_PER_OCTAVE;
    if (number < 0) {
        number += STANDARD_NOTATION_SEMITONES_PER_OCTAVE;
    }
    return number;
}

bool standard_notation_is_white_key(const PitchSpelling *spelling) {
    if (spelling == NULL) {
        return false;
    }
    switch (standard_notation_pitch_class(spelling)) {
        case 0:
        case 2:
        case 4:
        case 5:
        case 7:
        case 9:
        case 11:
            return true;
        default:
            return false;
    }
}

bool standard_notation_is_black_key(const PitchSpelling *spelling) {
    if (spelling == NULL) {
        return false;
    }
    return !standard_notation_is_white_key(spelling);
}
