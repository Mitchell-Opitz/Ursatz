#include "triad_rules.h"

#include <stddef.h>

static bool valid_quality(StandardTonalityTriadQuality quality) {
    return quality >= 0 && quality < STANDARD_TONALITY_TRIAD_QUALITY_COUNT;
}

bool standard_tonality_triad_quality_semitones(StandardTonalityTriadQuality quality, int out[3]) {
    static const int SEMITONES[STANDARD_TONALITY_TRIAD_QUALITY_COUNT][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8}};
    if (!valid_quality(quality) || out == NULL) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = SEMITONES[quality][i];
    }
    return true;
}
