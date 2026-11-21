#include "theory/tonality/chord_vocabulary.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/roman_numeral_rules.h"
#include "theory/tonality/triad_rules.h"

/* The labels of the quality-named triads (the last four, after the seven diatonic ones). */
static const char *const QUALITY_LABELS[COMMON_PRACTICE_TRIAD_COUNT - COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT] = {
    "augmented", "major", "minor", "diminished",
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

/* The label of a triad: a diatonic one is its Roman numeral (the degree in the case of its third, then the triad symbol: I, ii,
   vii with the degree sign), the others are named by quality. */
static void triad_label(CommonPracticeTriad triad, char *out, size_t size) {
    if (triad >= COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT) {
        snprintf(out, size, "%s", QUALITY_LABELS[triad - COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT]);
        return;
    }
    StandardTonalityTriadQuality quality = TRIAD_QUALITIES[triad];
    snprintf(out, size, "%s%s",
             standard_tonality_roman_numeral_degree(triad + 1, quality == STANDARD_TONALITY_TRIAD_MAJOR),
             standard_tonality_roman_numeral_triad_symbol(quality));
}

bool common_practice_triad_candidate(CommonPracticeTriad triad, unsigned long confidence_numerator,
                                     unsigned long confidence_denominator, ChordCandidate *out) {
    if (!valid_triad(triad) || out == NULL || confidence_denominator == 0 ||
        rational_make((long long)confidence_numerator, (long long)confidence_denominator, &out->confidence) != RATIONAL_OK) {
        return false;
    }
    triad_label(triad, out->label, sizeof(out->label));
    out->structure.root_pitch_class = -1;
    out->structure.bass_pitch_class = -1;
    out->structure.interval_count = 0;
    return true;
}

const char *common_practice_chord_symbol_suffix(const char *quality) {
    static const struct {
        const char *quality;
        const char *suffix;
    } SUFFIXES[] = {{"major", ""},          {"minor", "m"},   {"dominant7", "7"},
                    {"major7", "maj7"},     {"minor7", "m7"}, {"diminished", "dim"},
                    {"diminished7", "dim7"}, {"half-diminished7", "m7b5"}, {"augmented", "aug"}};
    if (quality == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(SUFFIXES) / sizeof(SUFFIXES[0]); i++) {
        if (strcmp(quality, SUFFIXES[i].quality) == 0) {
            return SUFFIXES[i].suffix;
        }
    }
    return quality;
}
