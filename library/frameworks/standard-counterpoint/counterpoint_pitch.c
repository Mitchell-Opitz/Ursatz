#include "counterpoint_pitch.h"

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

int standard_counterpoint_semitone_number(const PitchSpelling *spelling) {
    return STANDARD_COUNTERPOINT_SEMITONES_PER_OCTAVE * pitch_spelling_octave(spelling) +
           letter_semitone(pitch_spelling_letter(spelling)) + pitch_spelling_accidental(spelling);
}

int standard_counterpoint_step_number(const PitchSpelling *spelling) {
    return 7 * pitch_spelling_octave(spelling) + letter_step(pitch_spelling_letter(spelling));
}

int standard_counterpoint_direction(const PitchSpelling *from, const PitchSpelling *to) {
    int distance = standard_counterpoint_semitone_number(to) - standard_counterpoint_semitone_number(from);
    if (distance == 0) {
        distance = standard_counterpoint_step_number(to) - standard_counterpoint_step_number(from);
    }
    return (distance > 0) - (distance < 0);
}

bool standard_counterpoint_same_pitch(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return pitch_spelling_equals(a, b);
}
