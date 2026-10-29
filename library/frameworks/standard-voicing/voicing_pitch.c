#include "voicing_pitch.h"

#include <stddef.h>

#include "pitch/pitch_arithmetic.h"

int standard_voicing_semitone_number(const PitchSpelling *spelling) {
    return STANDARD_VOICING_SEMITONES_PER_OCTAVE * pitch_spelling_octave(spelling) +
           pitch_letter_semitone(pitch_spelling_letter(spelling)) + pitch_spelling_accidental(spelling);
}

int standard_voicing_step_number(const PitchSpelling *spelling) {
    return 7 * pitch_spelling_octave(spelling) + pitch_letter_index(pitch_spelling_letter(spelling));
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
