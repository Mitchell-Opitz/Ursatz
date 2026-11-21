#include "theory/tonality/chord_vocabulary.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/triad_rules.h"

/* vii\xC2\xB0 (diminished) carries the real degree symbol. */
static const char *const TRIAD_LABELS[COMMON_PRACTICE_TRIAD_COUNT] = {
    "I", "ii", "iii", "IV", "V", "vi", "vii\xC2\xB0", "augmented", "major", "minor", "diminished",
};

/* The quality of each triad; its intervals are the owner's (triad_rules.h). */
static const StandardTonalityTriadQuality TRIAD_QUALITIES[COMMON_PRACTICE_TRIAD_COUNT] = {
    STANDARD_TONALITY_TRIAD_MAJOR,      STANDARD_TONALITY_TRIAD_MINOR,  STANDARD_TONALITY_TRIAD_MINOR,
    STANDARD_TONALITY_TRIAD_MAJOR,      STANDARD_TONALITY_TRIAD_MAJOR,  STANDARD_TONALITY_TRIAD_MINOR,
    STANDARD_TONALITY_TRIAD_DIMINISHED, STANDARD_TONALITY_TRIAD_AUGMENTED, STANDARD_TONALITY_TRIAD_MAJOR,
    STANDARD_TONALITY_TRIAD_MINOR,      STANDARD_TONALITY_TRIAD_DIMINISHED,
};

static bool valid_triad(CommonPracticeTriad triad) {
    return triad >= 0 && triad < COMMON_PRACTICE_TRIAD_COUNT;
}

bool common_practice_triad_intervals(CommonPracticeTriad triad, int out[3]) {
    if (!valid_triad(triad) || out == NULL) {
        return false;
    }
    return standard_tonality_triad_quality_semitones(TRIAD_QUALITIES[triad], out);
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
