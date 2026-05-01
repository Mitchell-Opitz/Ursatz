#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "interpretation/key.h"
#include "interpretation/uncertainty.h"
#include "chord_vocabulary.h"
#include "chord_vocabulary_seventh.h"
#include "key_identification.h"
#include "persistence/persistence.h"
#include "persistence/persistence_chord.h"
#include "persistence/persistence_key.h"
#include "triad_identification.h"

/*
 * Round-trip test against real Chord/Key objects, built the same way
 * ChordIdentification/KeyEstimation (analysis/chord_identification.c,
 * analysis/key_estimation.c) build them -- via common-practice-minimal's
 * own identification helpers, which those analyzers call internally.
 */

static void assert_interpretations_equal(const Interpretation *a, const Interpretation *b) {
    assert(strcmp(entity_id_value(interpretation_id(a)), entity_id_value(interpretation_id(b))) == 0);
    assert(strcmp(entity_id_value(interpretation_target_id(a)), entity_id_value(interpretation_target_id(b))) == 0);
    assert(value_equals(interpretation_claim(a), interpretation_claim(b)));
    assert(interpretation_context_id(a) == NULL);
    assert(interpretation_context_id(b) == NULL);
    assert(interpretation_evidence_id(a) == NULL);
    assert(interpretation_evidence_id(b) == NULL);
    assert(framework_reference_equals(interpretation_framework_reference(a), interpretation_framework_reference(b)));
    assert(strcmp(interpretation_author(a), interpretation_author(b)) == 0);
    assert(strcmp(interpretation_method(a), interpretation_method(b)) == 0);
    assert(interpretation_status(a) == interpretation_status(b));
    assert(provenance_reference_equals(interpretation_provenance(a), interpretation_provenance(b)));

    const Uncertainty *uncertainty_a = interpretation_uncertainty(a);
    const Uncertainty *uncertainty_b = interpretation_uncertainty(b);
    assert(uncertainty_kind(uncertainty_a) == uncertainty_kind(uncertainty_b));
    if (uncertainty_kind(uncertainty_a) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE) {
        assert(uncertainty_scalar_confidence_numerator(uncertainty_a) ==
               uncertainty_scalar_confidence_numerator(uncertainty_b));
        assert(uncertainty_scalar_confidence_denominator(uncertainty_a) ==
               uncertainty_scalar_confidence_denominator(uncertainty_b));
    }
}

static void test_round_trip_chord(void) {
    EntityID *sonority_target_id = NULL;
    assert(entity_id_create("sonority-persistence-test", &sonority_target_id) == ENTITY_ID_OK);

    /* Scale degrees {1, 3, 5}: the tonic (I) triad. */
    int scale_degrees[] = {1, 3, 5};
    Chord *original = NULL;
    assert(common_practice_identify_triad_chord(scale_degrees, 3, sonority_target_id, &original));

    const char *path = "test_persistence_chord_round_trip.sqlite";
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    assert(persistence_store_chord(handle, original) == PERSISTENCE_CHORD_OK);

    const char *chord_id = entity_id_value(interpretation_id(chord_interpretation(original)));
    Chord *reloaded = NULL;
    assert(persistence_load_chord(handle, chord_id, &reloaded) == PERSISTENCE_CHORD_OK);

    assert(strcmp(chord_label(original), chord_label(reloaded)) == 0);
    assert_interpretations_equal(chord_interpretation(original), chord_interpretation(reloaded));

    chord_destroy(original);
    chord_destroy(reloaded);
    persistence_close(handle);
    remove(path);
}

/* The minor vocabulary's qualities (augmented triad, fully-diminished seventh) store and load like any other. */
static void test_round_trip_minor_qualities(void) {
    const char *path = "test_persistence_chord_minor_qualities.sqlite";
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    EntityID *augmented_target = NULL;
    assert(entity_id_create("sonority-persistence-augmented", &augmented_target) == ENTITY_ID_OK);
    Chord *augmented = NULL;
    assert(common_practice_build_triad_chord(COMMON_PRACTICE_TRIAD_AUGMENTED, augmented_target, &augmented) == CHORD_OK);
    EntityID *diminished_target = NULL;
    assert(entity_id_create("sonority-persistence-diminished7", &diminished_target) == ENTITY_ID_OK);
    Chord *diminished7 = NULL;
    assert(common_practice_build_seventh_chord(COMMON_PRACTICE_SEVENTH_DIMINISHED7, diminished_target, &diminished7) ==
           CHORD_OK);

    Chord *originals[2] = {augmented, diminished7};
    const char *labels[2] = {"augmented", "diminished7"};
    for (int i = 0; i < 2; i++) {
        assert(persistence_store_chord(handle, originals[i]) == PERSISTENCE_CHORD_OK);
        const char *chord_id = entity_id_value(interpretation_id(chord_interpretation(originals[i])));
        Chord *reloaded = NULL;
        assert(persistence_load_chord(handle, chord_id, &reloaded) == PERSISTENCE_CHORD_OK);
        assert(strcmp(chord_label(reloaded), labels[i]) == 0);
        assert_interpretations_equal(chord_interpretation(originals[i]), chord_interpretation(reloaded));
        chord_destroy(reloaded);
        chord_destroy(originals[i]);
    }
    persistence_close(handle);
    remove(path);
}

static void test_round_trip_key(void) {
    EntityID *context_target_id = NULL;
    assert(entity_id_create("context-persistence-test", &context_target_id) == ENTITY_ID_OK);

    /* C major: W-W-H-W-W-W-H from tonic. */
    int pitch_classes[] = {0, 2, 4, 5, 7, 9, 11};
    Key *original = NULL;
    assert(common_practice_identify_key(pitch_classes, 7, "C", context_target_id, &original));

    const char *path = "test_persistence_key_round_trip.sqlite";
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    assert(persistence_store_key(handle, original) == PERSISTENCE_KEY_OK);

    const char *key_id = entity_id_value(interpretation_id(key_interpretation(original)));
    Key *reloaded = NULL;
    assert(persistence_load_key(handle, key_id, &reloaded) == PERSISTENCE_KEY_OK);

    assert(strcmp(key_label(original), key_label(reloaded)) == 0);
    assert_interpretations_equal(key_interpretation(original), key_interpretation(reloaded));

    key_destroy(original);
    key_destroy(reloaded);
    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_round_trip_chord();
    test_round_trip_minor_qualities();
    test_round_trip_key();
    printf("persistence chord/key: all tests passed\n");
    return 0;
}
