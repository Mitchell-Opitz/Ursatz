#include "spacing_rules.h"

#include <stddef.h>

#include "voicing_pitch.h"

int standard_voicing_spacing_limit(VoicePart upper) {
    switch (upper) {
        case VOICE_PART_SOPRANO:
        case VOICE_PART_ALTO:
            return STANDARD_VOICING_UPPER_SPACING_LIMIT;
        case VOICE_PART_TENOR:
            return STANDARD_VOICING_LOWER_SPACING_LIMIT;
        default:
            return 0;
    }
}

bool standard_voicing_spacing_valid(VoicePart upper, const PitchSpelling *upper_pitch,
                                    const PitchSpelling *lower_pitch) {
    int limit = standard_voicing_spacing_limit(upper);
    if (limit == 0 || upper_pitch == NULL || lower_pitch == NULL) {
        return false;
    }
    return standard_voicing_generic_size(upper_pitch, lower_pitch) <= limit;
}

VoicePart standard_voicing_first_wide_spacing(const StandardVoicingChord *chord) {
    if (chord == NULL) {
        return VOICE_PART_COUNT;
    }
    for (int i = VOICE_PART_SOPRANO; i < VOICE_PART_BASS; i++) {
        if (!standard_voicing_spacing_valid((VoicePart)i, chord->voice[i], chord->voice[i + 1])) {
            return (VoicePart)i;
        }
    }
    return VOICE_PART_COUNT;
}

bool standard_voicing_chord_spacing_valid(const StandardVoicingChord *chord) {
    return chord != NULL && standard_voicing_first_wide_spacing(chord) == VOICE_PART_COUNT;
}
