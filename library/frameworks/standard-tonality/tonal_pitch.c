#include "tonal_pitch.h"

int standard_tonality_letter_semitone(PitchLetter letter) {
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

int standard_tonality_semitone_number(const PitchSpelling *spelling) {
    return STANDARD_TONALITY_SEMITONES_PER_OCTAVE * pitch_spelling_octave(spelling) +
           standard_tonality_letter_semitone(pitch_spelling_letter(spelling)) + pitch_spelling_accidental(spelling);
}

PitchLetter standard_tonality_next_letter(PitchLetter letter) {
    switch (letter) {
        case PITCH_LETTER_C:
            return PITCH_LETTER_D;
        case PITCH_LETTER_D:
            return PITCH_LETTER_E;
        case PITCH_LETTER_E:
            return PITCH_LETTER_F;
        case PITCH_LETTER_F:
            return PITCH_LETTER_G;
        case PITCH_LETTER_G:
            return PITCH_LETTER_A;
        case PITCH_LETTER_A:
            return PITCH_LETTER_B;
        case PITCH_LETTER_B:
        default:
            return PITCH_LETTER_C;
    }
}
