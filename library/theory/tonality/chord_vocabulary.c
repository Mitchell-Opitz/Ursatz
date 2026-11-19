#include "theory/tonality/chord_vocabulary.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>


/* vii\xC2\xB0 (diminished) carries the real degree symbol. */
static const char *const TRIAD_LABELS[COMMON_PRACTICE_TRIAD_COUNT] = {
    "I", "ii", "iii", "IV", "V", "vi", "vii\xC2\xB0", "augmented", "major", "minor", "diminished",
};

static const int TRIAD_INTERVALS[COMMON_PRACTICE_TRIAD_COUNT][3] = {
    {0, 4, 7}, {0, 3, 7}, {0, 3, 7}, {0, 4, 7}, {0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8},
    {0, 4, 7}, {0, 3, 7}, {0, 3, 6},
};

static bool valid_triad(CommonPracticeTriad triad) {
    return triad >= 0 && triad < COMMON_PRACTICE_TRIAD_COUNT;
}

bool common_practice_triad_intervals(CommonPracticeTriad triad, int out[3]) {
    if (!valid_triad(triad) || out == NULL) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = TRIAD_INTERVALS[triad][i];
    }
    return true;
}

bool common_practice_triad_candidate(CommonPracticeTriad triad, unsigned long confidence_numerator,
                                     unsigned long confidence_denominator, ChordCandidate *out) {
    if (!valid_triad(triad) || out == NULL || confidence_denominator == 0 ||
        rational_make((long long)confidence_numerator, (long long)confidence_denominator, &out->confidence) != RATIONAL_OK) {
        return false;
    }
    snprintf(out->label, sizeof(out->label), "%s", TRIAD_LABELS[triad]);
    out->structure.root_pitch_class = -1;
    out->structure.bass_pitch_class = -1;
    out->structure.interval_count = 0;
    return true;
}
