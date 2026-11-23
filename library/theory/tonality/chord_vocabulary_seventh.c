#include "theory/tonality/chord_vocabulary_seventh.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/seventh_rules.h"

static const char *const SEVENTH_LABELS[COMMON_PRACTICE_SEVENTH_COUNT] = {
    "dominant7", "major7", "minor7", "half-diminished7", "diminished7",
};

/* The quality of each seventh; its intervals are the owner's (seventh_rules.h). */
static const StandardTonalitySeventhQuality SEVENTH_QUALITIES[COMMON_PRACTICE_SEVENTH_COUNT] = {
    STANDARD_TONALITY_SEVENTH_MAJOR_MINOR,    STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR,
    STANDARD_TONALITY_SEVENTH_MINOR_MINOR,    STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED,
    STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED,
};

static bool valid_seventh(CommonPracticeSeventh seventh) {
    return seventh >= 0 && seventh < COMMON_PRACTICE_SEVENTH_COUNT;
}

bool common_practice_seventh_intervals(CommonPracticeSeventh seventh, int out[4]) {
    if (!valid_seventh(seventh) || out == NULL) {
        return false;
    }
    return standard_tonality_seventh_quality_semitones(SEVENTH_QUALITIES[seventh], out);
}

bool common_practice_seventh_intervals_with(const CommonPracticeMaterials *materials, CommonPracticeSeventh seventh, int out[4]) {
    if (!valid_seventh(seventh) || out == NULL) {
        return false;
    }
    return common_practice_seventh_semitones(materials, SEVENTH_QUALITIES[seventh], out);
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
