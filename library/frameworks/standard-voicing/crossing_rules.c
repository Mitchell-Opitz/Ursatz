#include "crossing_rules.h"

#include <stddef.h>

#include "voicing_pitch.h"

bool standard_voicing_pair_ordered(const PitchSpelling *upper, const PitchSpelling *lower) {
    if (upper == NULL || lower == NULL) {
        return false;
    }
    return standard_voicing_semitone_number(upper) > standard_voicing_semitone_number(lower);
}

VoicePart standard_voicing_first_crossing(const StandardVoicingChord *chord) {
    if (chord == NULL) {
        return VOICE_PART_COUNT;
    }
    for (int i = VOICE_PART_SOPRANO; i < VOICE_PART_BASS; i++) {
        if (!standard_voicing_pair_ordered(chord->voice[i], chord->voice[i + 1])) {
            return (VoicePart)i;
        }
    }
    return VOICE_PART_COUNT;
}

bool standard_voicing_chord_uncrossed(const StandardVoicingChord *chord) {
    return chord != NULL && standard_voicing_first_crossing(chord) == VOICE_PART_COUNT;
}
