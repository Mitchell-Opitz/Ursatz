#include "range_rules.h"

#include <stddef.h>

#include "voicing_pitch.h"

typedef struct {
    PitchLetter low_letter;
    int low_octave;
    PitchLetter high_letter;
    int high_octave;
} RangeBounds;

static const RangeBounds RANGES[VOICE_PART_COUNT] = {
    {PITCH_LETTER_C, 4, PITCH_LETTER_G, 5}, /* soprano */
    {PITCH_LETTER_G, 3, PITCH_LETTER_D, 5}, /* alto */
    {PITCH_LETTER_C, 3, PITCH_LETTER_G, 4}, /* tenor */
    {PITCH_LETTER_F, 2, PITCH_LETTER_D, 4}, /* bass */
};

static int natural_semitones(PitchLetter letter, int octave) {
    PitchSpelling *spelling = NULL;
    if (pitch_spelling_create(letter, 0, octave, &spelling) != PITCH_SPELLING_OK) {
        return 0;
    }
    int number = standard_voicing_semitone_number(spelling);
    pitch_spelling_destroy(spelling);
    return number;
}

bool standard_voicing_range_semitones(VoicePart part, int *lowest, int *highest) {
    if (lowest == NULL || highest == NULL || !voice_part_valid(part)) {
        return false;
    }
    *lowest = natural_semitones(RANGES[part].low_letter, RANGES[part].low_octave);
    *highest = natural_semitones(RANGES[part].high_letter, RANGES[part].high_octave);
    return true;
}

bool standard_voicing_range_bounds(VoicePart part, PitchLetter *lowest_letter, int *lowest_octave,
                                   PitchLetter *highest_letter, int *highest_octave) {
    if (lowest_letter == NULL || lowest_octave == NULL || highest_letter == NULL || highest_octave == NULL ||
        !voice_part_valid(part)) {
        return false;
    }
    *lowest_letter = RANGES[part].low_letter;
    *lowest_octave = RANGES[part].low_octave;
    *highest_letter = RANGES[part].high_letter;
    *highest_octave = RANGES[part].high_octave;
    return true;
}

bool standard_voicing_in_range(VoicePart part, const PitchSpelling *pitch) {
    int lowest;
    int highest;
    if (pitch == NULL || !standard_voicing_range_semitones(part, &lowest, &highest)) {
        return false;
    }
    int number = standard_voicing_semitone_number(pitch);
    return number >= lowest && number <= highest;
}

bool standard_voicing_chord_in_range(const StandardVoicingChord *chord) {
    if (chord == NULL) {
        return false;
    }
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        if (!standard_voicing_in_range((VoicePart)i, chord->voice[i])) {
            return false;
        }
    }
    return true;
}
