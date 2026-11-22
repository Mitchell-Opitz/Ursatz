#ifndef URSATZ_STANDARD_TONALITY_TESTS_ADAPTER_TEST_UTIL_H
#define URSATZ_STANDARD_TONALITY_TESTS_ADAPTER_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/harmony/chord_input.h"
#include <stdlib.h>

#include "model/core/rational.h"
#include "theory/tonality/chord_vocabulary.h"
#include "theory/tonality/key_vocabulary.h"
#include "model/time/musical_time.h"
#include "ursatz_test_knowledge.h"

/* Builders for the chord candidates and keys the adapter reads. They stand in for a producer such as
   common-practice-minimal, which standard-tonality does not include. */

/* A chord candidate over the Sonority `segment` with the given structure and chord confidence in millionths. The caller frees it. */
static ChordCandidate *adapter_test_chord(const char *segment, int root, int bass, const int *intervals, size_t count,
                                          unsigned long confidence_millionths) {
    (void)segment;
    ChordCandidate *chord = (ChordCandidate *)calloc(1, sizeof(*chord));
    assert(chord != NULL);
    snprintf(chord->label, sizeof(chord->label), "chord");
    assert(rational_make((long long)confidence_millionths, 1000000LL, &chord->confidence) == RATIONAL_OK);
    chord->structure.root_pitch_class = root;
    chord->structure.bass_pitch_class = bass;
    chord->structure.interval_count = count;
    for (size_t i = 0; i < count; i++) {
        chord->structure.root_relative_intervals[i] = intervals[i];
    }
    return chord;
}

static KeyStructure adapter_test_key(const char *name, int tonic, CommonPracticeKeyMode mode) {
    (void)name;
    KeyStructure structure = {tonic, common_practice_material(test_materials(), mode)};
    return structure;
}

static MusicalTime adapter_test_time(long numerator, unsigned long denominator) {
    MusicalTime time;
    assert(musical_time_make(numerator, (long long)denominator, &time) == MUSICAL_TIME_OK);
    return time;
}

static const int ADAPTER_MAJOR_TRIAD[3] = {0, 4, 7};
static const int ADAPTER_MINOR_TRIAD[3] = {0, 3, 7};
static const int ADAPTER_DIM_TRIAD[3] = {0, 3, 6};
static const int ADAPTER_DOMINANT_SEVENTH[4] = {0, 4, 7, 10};

#endif
