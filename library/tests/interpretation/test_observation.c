#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/observation.h"
#include "interpretation/uncertainty.h"

/*
 * Context test: Observation stores raw detected values, not
 * conclusions. This test confirms construction succeeds with a
 * detected value/method/source and an Uncertainty, and confirms
 * Observation carries no interpretive-claim field or accessor -- only
 * observation_value_or_candidates (raw) is exposed, nothing resembling
 * assertion_claim (interpretation/assertion.h).
 */

int main(void) {
    EntityID *id;
    assert(entity_id_create("observation-1", &id) == ENTITY_ID_OK);
    EntityID *subject_id;
    assert(entity_id_create("note-1", &subject_id) == ENTITY_ID_OK);

    const int detected_midi_number = 60;
    Value *detected_value;
    assert(value_create("DetectedPitch", &detected_midi_number, sizeof(detected_midi_number), &detected_value) ==
           VALUE_OK);

    Uncertainty *uncertainty;
    assert(uncertainty_create_scalar_confidence(9, 10, &uncertainty) == UNCERTAINTY_OK);

    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Observation *observation;
    ObservationResult result = observation_create(id, subject_id, "pitch", detected_value, "pitch-tracker-v3",
                                                    "audio-recording-42", uncertainty, NULL, provenance,
                                                    &observation);
    assert(result == OBSERVATION_OK);

    assert(observation_subject_id(observation) == subject_id);
    assert(strcmp(observation_property(observation), "pitch") == 0);

    size_t value_size;
    const void *detected_content = value_content(observation_value_or_candidates(observation), &value_size);
    assert(value_size == sizeof(detected_midi_number));
    assert(memcmp(detected_content, &detected_midi_number, value_size) == 0);

    assert(strcmp(observation_method(observation), "pitch-tracker-v3") == 0);
    assert(strcmp(observation_source(observation), "audio-recording-42") == 0);
    assert(uncertainty_kind(observation_uncertainty(observation)) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(observation_timestamp(observation) == NULL);

    observation_destroy(observation);

    /* A missing (NULL) uncertainty is rejected -- every measurement states
       its own uncertainty, even if that state is "unknown". */
    EntityID *id_2;
    assert(entity_id_create("observation-2", &id_2) == ENTITY_ID_OK);
    EntityID *subject_id_2;
    assert(entity_id_create("note-2", &subject_id_2) == ENTITY_ID_OK);
    Value *detected_value_2;
    assert(value_create("DetectedPitch", &detected_midi_number, sizeof(detected_midi_number), &detected_value_2) ==
           VALUE_OK);
    ProvenanceReference *provenance_2;
    assert(provenance_reference_create_absent(&provenance_2) == PROVENANCE_REFERENCE_OK);

    Observation *rejected;
    assert(observation_create(id_2, subject_id_2, "pitch", detected_value_2, "pitch-tracker-v3",
                               "audio-recording-42", NULL, NULL, provenance_2,
                               &rejected) == OBSERVATION_ERR_NULL_UNCERTAINTY);

    entity_id_destroy(id_2);
    entity_id_destroy(subject_id_2);
    value_destroy(detected_value_2);
    provenance_reference_destroy(provenance_2);

    printf("test_observation: ok\n");
    return 0;
}