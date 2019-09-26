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

/*
 * Relationship test: a Chord's underlying fact reference (the Sonority
 * it is about) resolves through its embedded Interpretation's own
 * target_id -- no separate sonority_id field exists on Chord itself.
 * A Chord requires a non-NULL framework_reference, unlike the base
 * Interpretation type where it is optional.
 */

static Value *make_claim(const char *content) {
    Value *claim;
    assert(value_create("Claim", content, strlen(content) + 1, &claim) == VALUE_OK);
    return claim;
}

static Uncertainty *make_uncertainty(void) {
    Uncertainty *uncertainty;
    assert(uncertainty_create_scalar_confidence(1, 1, &uncertainty) == UNCERTAINTY_OK);
    return uncertainty;
}

static Interpretation *make_interpretation(const char *own_id, const char *sonority_id, const char *framework_id) {
    EntityID *id;
    assert(entity_id_create(own_id, &id) == ENTITY_ID_OK);
    EntityID *target_id;
    assert(entity_id_create(sonority_id, &target_id) == ENTITY_ID_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    FrameworkReference *framework_reference = NULL;
    if (framework_id != NULL) {
        EntityID *framework_entity_id;
        assert(entity_id_create(framework_id, &framework_entity_id) == ENTITY_ID_OK);
        assert(framework_reference_create(framework_entity_id, &framework_reference) == FRAMEWORK_REFERENCE_OK);
    }

    Interpretation *interpretation;
    assert(interpretation_create(id, target_id, make_claim("V7"), NULL, NULL, framework_reference,
                                  make_uncertainty(), "analyst-jane", "roman-numeral-analysis",
                                  INTERPRETATION_STATUS_PROPOSED, provenance, &interpretation) == INTERPRETATION_OK);
    return interpretation;
}

int main(void) {
    Interpretation *interpretation = make_interpretation("interpretation-1", "sonority-1", "framework-common-practice");

    Chord *chord;
    assert(chord_create(interpretation, "V7", &chord) == CHORD_OK);

    assert(chord_interpretation(chord) == interpretation);
    assert(strcmp(chord_label(chord), "V7") == 0);

    EntityID *expected_sonority_id;
    assert(entity_id_create("sonority-1", &expected_sonority_id) == ENTITY_ID_OK);
    assert(entity_id_equals(interpretation_target_id(chord_interpretation(chord)), expected_sonority_id));
    entity_id_destroy(expected_sonority_id);

    chord_destroy(chord);

    /* A framework_reference is required: unlike the base Interpretation
       type, chord_create rejects an interpretation without one. */
    Interpretation *no_framework = make_interpretation("interpretation-2", "sonority-2", NULL);
    Chord *rejected;
    assert(chord_create(no_framework, "V7", &rejected) == CHORD_ERR_NULL_FRAMEWORK_REFERENCE);
    interpretation_destroy(no_framework);

    printf("test_chord: ok\n");
    return 0;
}