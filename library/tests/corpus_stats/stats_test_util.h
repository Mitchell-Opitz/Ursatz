#ifndef URSATZ_TESTS_CORPUS_STATS_STATS_TEST_UTIL_H
#define URSATZ_TESTS_CORPUS_STATS_STATS_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "persistence/persistence.h"
#include "persistence/persistence_chord.h"
#include "persistence/persistence_interpretation.h"

/*
 * Builders for persisted interpretations that belong to a piece: a piece is a
 * context, so each interpretation carries its piece's id as its context_id.
 * Every id passed in must be distinct (it is a PRIMARY KEY).
 */

static inline Interpretation *stats_make_interpretation(const char *id_value, const char *claim_type, const void *claim_bytes,
                                                 size_t claim_size, const char *context_value) {
    EntityID *id = NULL;
    assert(entity_id_create(id_value, &id) == ENTITY_ID_OK);
    EntityID *target_id = NULL;
    assert(entity_id_create("test-target", &target_id) == ENTITY_ID_OK);
    Value *claim = NULL;
    assert(value_create(claim_type, claim_bytes, claim_size, &claim) == VALUE_OK);
    EntityID *context_id = NULL;
    if (context_value != NULL) {
        assert(entity_id_create(context_value, &context_id) == ENTITY_ID_OK);
    }
    EntityID *framework_target_id = NULL;
    assert(entity_id_create("test-framework", &framework_target_id) == ENTITY_ID_OK);
    FrameworkReference *framework_reference = NULL;
    assert(framework_reference_create(framework_target_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);
    Uncertainty *uncertainty = NULL;
    assert(uncertainty_create_unknown(&uncertainty) == UNCERTAINTY_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Interpretation *interpretation = NULL;
    assert(interpretation_create(id, target_id, claim, context_id, NULL, framework_reference, uncertainty,
                                 "test-author", "test-method", INTERPRETATION_STATUS_MACHINE_GENERATED, provenance,
                                 &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

/* Stores a Chord labelled `label` in the piece `piece`. */
static inline void stats_store_chord(PersistenceHandle *persistence, const char *id_value, const char *label,
                              const char *piece) {
    Interpretation *interpretation =
        stats_make_interpretation(id_value, "chord-label", label, strlen(label) + 1, piece);
    Chord *chord = NULL;
    assert(chord_create(interpretation, label, &chord) == CHORD_OK);
    assert(persistence_store_chord(persistence, chord) == PERSISTENCE_CHORD_OK);
    chord_destroy(chord);
}

/* Stores a generic interpretation with the given claim in the piece `piece`. */
static inline void stats_store_claim(PersistenceHandle *persistence, const char *id_value, const char *claim_type,
                              const void *claim_bytes, size_t claim_size, const char *piece) {
    Interpretation *interpretation = stats_make_interpretation(id_value, claim_type, claim_bytes, claim_size, piece);
    assert(persistence_store_interpretation(persistence, interpretation) == PERSISTENCE_INTERPRETATION_OK);
    interpretation_destroy(interpretation);
}

#endif
