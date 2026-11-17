#include "theory/voicing/doubling_rules.h"

#include "theory/voicing/construction_rules.h"
#include "model/pitch/pitch_arithmetic.h"

static bool chord_complete(const StandardVoicingChord *chord) {
    if (chord == NULL) {
        return false;
    }
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        if (chord->voice[i] == NULL) {
            return false;
        }
    }
    return true;
}

static bool valid_tones(const PitchSpelling *const *tones, size_t tone_count) {
    if (tones == NULL ||
        (tone_count != STANDARD_VOICING_TRIAD_TONES && tone_count != STANDARD_VOICING_SEVENTH_TONES)) {
        return false;
    }
    for (size_t i = 0; i < tone_count; i++) {
        if (tones[i] == NULL) {
            return false;
        }
    }
    return true;
}

int standard_voicing_pitch_class_count(const StandardVoicingChord *chord, const PitchSpelling *note) {
    if (chord == NULL || note == NULL) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        if (pitch_spelling_same_name(chord->voice[i], note)) {
            count++;
        }
    }
    return count;
}

bool standard_voicing_leading_tone_undoubled(const StandardVoicingChord *chord, const PitchSpelling *leading_tone) {
    if (!chord_complete(chord) || leading_tone == NULL) {
        return false;
    }
    return standard_voicing_pitch_class_count(chord, leading_tone) <= 1;
}

bool standard_voicing_seventh_undoubled(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                        size_t tone_count) {
    if (!chord_complete(chord) || !valid_tones(tones, tone_count)) {
        return false;
    }
    if (tone_count != STANDARD_VOICING_SEVENTH_TONES) {
        return true;
    }
    return standard_voicing_pitch_class_count(chord, tones[3]) <= 1;
}

bool standard_voicing_seventh_chord_undoubled(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                              size_t tone_count) {
    if (!chord_complete(chord) || !valid_tones(tones, tone_count)) {
        return false;
    }
    if (tone_count != STANDARD_VOICING_SEVENTH_TONES) {
        return true;
    }
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        if (standard_voicing_pitch_class_count(chord, chord->voice[i]) > 1) {
            return false;
        }
    }
    return true;
}

bool standard_voicing_doubling_valid(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                     size_t tone_count, const PitchSpelling *leading_tone) {
    if (!chord_complete(chord) || !valid_tones(tones, tone_count)) {
        return false;
    }
    if (leading_tone != NULL && !standard_voicing_leading_tone_undoubled(chord, leading_tone)) {
        return false;
    }
    return standard_voicing_seventh_undoubled(chord, tones, tone_count) &&
           standard_voicing_seventh_chord_undoubled(chord, tones, tone_count);
}
