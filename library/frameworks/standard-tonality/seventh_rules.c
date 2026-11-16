#include "seventh_rules.h"

#include <stddef.h>

bool standard_tonality_seventh_quality_semitones(StandardTonalitySeventhQuality quality, int out[4]) {
    static const int SEMITONES[STANDARD_TONALITY_SEVENTH_QUALITY_COUNT][4] = {
        {0, 4, 7, 11}, {0, 4, 7, 10}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}};
    if (quality < 0 || quality >= STANDARD_TONALITY_SEVENTH_QUALITY_COUNT || out == NULL) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        out[i] = SEMITONES[quality][i];
    }
    return true;
}
