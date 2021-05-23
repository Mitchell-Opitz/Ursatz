#include "construction_rules.h"

#include <string.h>

#include "voicing_pitch.h"

static bool valid_input(const StandardVoicingChord *chord, const PitchSpelling *const *tones, size_t tone_count,
                        int inversion) {
    if (chord == NULL || tones == NULL) {
        return false;
    }
    if (tone_count != STANDARD_VOICING_TRIAD_TONES && tone_count != STANDARD_VOICING_SEVENTH_TONES) {
        return false;
    }
    if (inversion < 0 || (size_t)inversion >= tone_count) {
        return false;
    }
    for (size_t i = 0; i < tone_count; i++) {
        if (tones[i] == NULL) {
            return false;
        }
    }
    for (int i = 0; i < VOICE_PART_COUNT; i++) {
        if (chord->voice[i] == NULL) {
            return false;
        }
    }
    return true;
}

StandardVoicingConstruction standard_voicing_check_construction(const StandardVoicingChord *chord,
                                                                const PitchSpelling *const *tones,
                                                                size_t tone_count, int inversion) {
    if (!valid_input(chord, tones, tone_count, inversion)) {
        return STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT;
    }

    if (!standard_voicing_same_pitch_class(chord->voice[VOICE_PART_BASS], tones[inversion])) {
        return STANDARD_VOICING_CONSTRUCTION_BASS_MISMATCH;
    }

    for (int v = 0; v < VOICE_PART_COUNT; v++) {
        bool is_tone = false;
        for (size_t t = 0; t < tone_count; t++) {
            if (standard_voicing_same_pitch_class(chord->voice[v], tones[t])) {
                is_tone = true;
                break;
            }
        }
        if (!is_tone) {
            return STANDARD_VOICING_CONSTRUCTION_FOREIGN_NOTE;
        }
    }

    for (size_t t = 0; t < tone_count; t++) {
        bool sounds = false;
        for (int v = 0; v < VOICE_PART_COUNT; v++) {
            if (standard_voicing_same_pitch_class(chord->voice[v], tones[t])) {
                sounds = true;
                break;
            }
        }
        if (!sounds) {
            return STANDARD_VOICING_CONSTRUCTION_MISSING_TONE;
        }
    }

    return STANDARD_VOICING_CONSTRUCTION_OK;
}

typedef struct {
    const char *figure;
    size_t chord_size;
    int inversion;
} FigureEntry;

static const FigureEntry FIGURES[] = {
    {"", 3, 0},   {"5", 3, 0},  {"53", 3, 0}, {"6", 3, 1},  {"63", 3, 1}, {"64", 3, 2},
    {"7", 4, 0},  {"65", 4, 1}, {"43", 4, 2}, {"42", 4, 3}, {"2", 4, 3},
};

bool standard_voicing_inversion_from_figure(const char *figure, size_t chord_size, int *inversion) {
    if (figure == NULL || inversion == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        if (FIGURES[i].chord_size == chord_size && strcmp(FIGURES[i].figure, figure) == 0) {
            *inversion = FIGURES[i].inversion;
            return true;
        }
    }
    return false;
}
