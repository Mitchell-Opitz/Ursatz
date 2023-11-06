#ifndef URSATZ_STANDARD_TONALITY_TESTS_ADAPTER_TEST_UTIL_H
#define URSATZ_STANDARD_TONALITY_TESTS_ADAPTER_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chord_input.h"
#include "interpretation/chord.h"
#include "interpretation/key.h"
#include "time/musical_time.h"

/* Builders for the kernel Chords and Keys the adapter reads. They stand in for a producer such as
   common-practice-minimal, which standard-tonality does not include. */

static EntityID *adapter_test_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Interpretation *adapter_test_interpretation(const char *id, const char *target, unsigned long confidence_millionths) {
    Value *claim = NULL;
    assert(value_create("test/claim", "x", 2, &claim) == VALUE_OK);
    FrameworkReference *framework = NULL;
    assert(framework_reference_create(adapter_test_id("test-framework"), &framework) == FRAMEWORK_REFERENCE_OK);
    Uncertainty *uncertainty = NULL;
    assert(uncertainty_create_scalar_confidence(confidence_millionths, 1000000UL, &uncertainty) == UNCERTAINTY_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    Interpretation *interpretation = NULL;
    assert(interpretation_create(adapter_test_id(id), adapter_test_id(target), claim, NULL, NULL, framework,
                                 uncertainty, "test", "test", INTERPRETATION_STATUS_MACHINE_GENERATED, provenance,
                                 &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

/* A Chord over the Sonority `segment` with the given structure and chord confidence in millionths. */
static Chord *adapter_test_chord(const char *segment, int root, int bass, const int *intervals, size_t count,
                                 unsigned long confidence_millionths) {
    char id[128];
    snprintf(id, sizeof(id), "chord-%s", segment);
    ChordStructure structure;
    structure.root_pitch_class = root;
    structure.bass_pitch_class = bass;
    structure.interval_count = count;
    for (size_t i = 0; i < count; i++) {
        structure.root_relative_intervals[i] = intervals[i];
    }
    Chord *chord = NULL;
    assert(chord_create_with_structure(adapter_test_interpretation(id, segment, confidence_millionths), "chord",
                                       &structure, &chord) == CHORD_OK);
    return chord;
}

static Key *adapter_test_key(const char *name, int tonic, KeyStructureMode mode) {
    KeyStructure structure = {tonic, mode};
    Key *key = NULL;
    assert(key_create_with_structure(adapter_test_interpretation(name, name, 1000000UL), name, &structure, &key) ==
           KEY_OK);
    return key;
}

static MusicalTime *adapter_test_time(long numerator, unsigned long denominator) {
    MusicalTime *time = NULL;
    assert(musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK);
    return time;
}

static const int ADAPTER_MAJOR_TRIAD[3] = {0, 4, 7};
static const int ADAPTER_MINOR_TRIAD[3] = {0, 3, 7};
static const int ADAPTER_DIM_TRIAD[3] = {0, 3, 6};
static const int ADAPTER_DOMINANT_SEVENTH[4] = {0, 4, 7, 10};

#endif
