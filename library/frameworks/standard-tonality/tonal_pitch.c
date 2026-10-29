#include "tonal_pitch.h"

#include "pitch/pitch_arithmetic.h"

int standard_tonality_letter_semitone(PitchLetter letter) {
    return pitch_letter_semitone(letter);
}

int standard_tonality_semitone_number(const PitchSpelling *spelling) {
    return STANDARD_TONALITY_SEMITONES_PER_OCTAVE * pitch_spelling_octave(spelling) +
           standard_tonality_letter_semitone(pitch_spelling_letter(spelling)) + pitch_spelling_accidental(spelling);
}

PitchLetter standard_tonality_next_letter(PitchLetter letter) {
    return pitch_letter_offset(letter, 1);
}
