#include "theory/tonality/chord_vocabulary_seventh.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>


static const char *const SEVENTH_LABELS[COMMON_PRACTICE_SEVENTH_COUNT] = {
    "dominant7", "major7", "minor7", "half-diminished7", "diminished7",
};

static const int SEVENTH_INTERVALS[COMMON_PRACTICE_SEVENTH_COUNT][4] = {
    {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9},
};

static bool valid_seventh(CommonPracticeSeventh seventh) {
    return seventh >= 0 && seventh < COMMON_PRACTICE_SEVENTH_COUNT;
}

bool common_practice_seventh_intervals(CommonPracticeSeventh seventh, int out[4]) {
    if (!valid_seventh(seventh) || out == NULL) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        out[i] = SEVENTH_INTERVALS[seventh][i];
    }
    return true;
}

const char *common_practice_seventh_label(CommonPracticeSeventh seventh) {
    if (!valid_seventh(seventh)) {
        return NULL;
    }
    return SEVENTH_LABELS[seventh];
}

bool common_practice_seventh_candidate(CommonPracticeSeventh seventh, unsigned long confidence_numerator,
                                       unsigned long confidence_denominator, ChordCandidate *out) {
    if (!valid_seventh(seventh) || out == NULL || confidence_denominator == 0 ||
        rational_make((long long)confidence_numerator, (long long)confidence_denominator, &out->confidence) != RATIONAL_OK) {
        return false;
    }
    snprintf(out->label, sizeof(out->label), "%s", SEVENTH_LABELS[seventh]);
    out->structure.root_pitch_class = -1;
    out->structure.bass_pitch_class = -1;
    out->structure.interval_count = 0;
    return true;
}
