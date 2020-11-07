#include "scale_pattern.h"

#include <stdlib.h>

#include "tonal_pitch.h"

bool standard_tonality_is_scale_pattern(const int *steps, size_t step_count, const PitchSpelling *const *notes,
                                        size_t count) {
    if (steps == NULL || notes == NULL || count != step_count + 1) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i] == NULL) {
            return false;
        }
    }

    PitchLetter letter = pitch_spelling_letter(notes[0]);
    for (size_t i = 0; i < step_count; i++) {
        letter = standard_tonality_next_letter(letter);
        if (pitch_spelling_letter(notes[i + 1]) != letter) {
            return false;
        }
        int distance = standard_tonality_semitone_number(notes[i + 1]) - standard_tonality_semitone_number(notes[i]);
        if (distance != steps[i]) {
            return false;
        }
    }
    return true;
}

bool standard_tonality_build_scale_pattern(const int *steps, size_t step_count, const PitchSpelling *tonic,
                                           PitchSpelling **out) {
    if (steps == NULL || tonic == NULL || out == NULL) {
        return false;
    }

    size_t count = step_count + 1;
    PitchSpelling **built = (PitchSpelling **)malloc(sizeof(PitchSpelling *) * count);
    if (built == NULL) {
        return false;
    }

    PitchLetter letter = pitch_spelling_letter(tonic);
    int letter_octave = pitch_spelling_octave(tonic);
    int target = standard_tonality_semitone_number(tonic);
    size_t created = 0;

    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            PitchLetter next = standard_tonality_next_letter(letter);
            if (next == PITCH_LETTER_C) {
                letter_octave++;
            }
            letter = next;
            target += steps[i - 1];
        }
        int natural = STANDARD_TONALITY_SEMITONES_PER_OCTAVE * letter_octave + standard_tonality_letter_semitone(letter);
        int accidental = target - natural;
        if (accidental < -2 || accidental > 2) {
            goto fail;
        }
        if (pitch_spelling_create(letter, accidental, letter_octave, &built[i]) != PITCH_SPELLING_OK) {
            goto fail;
        }
        created++;
    }

    for (size_t i = 0; i < count; i++) {
        out[i] = built[i];
    }
    free(built);
    return true;

fail:
    for (size_t i = 0; i < created; i++) {
        pitch_spelling_destroy(built[i]);
    }
    free(built);
    return false;
}
