#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/phrase_boundary_detection.h"
#include "semantics/cadence_type.h"
#include "containers/phrase.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Behavioral-contract test: PhraseBoundaryDetection's two AnalyzerFn
 * entry points (plain boundary delegation to PhraseSegmentation, and
 * the signal-recording claim) each reject NULL input without touching
 * *out and yield a valid empty AnalysisResult for an empty point list.
 *
 * Analysis-regression test: a fixed five-point sequence exercises both
 * boundary signals (cadence, including "unclassified"; contour/rhythm
 * discontinuity once max_length_without_cadence elapses) under the
 * primary/secondary signal rule, and pins PhraseOccurrence's resulting
 * phrase grouping (including the trailing partial phrase) against a
 * future refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static TemporalAnchor *make_temporal_anchor(long numerator, unsigned long denominator) {
    MusicalTime *position;
    assert(musical_time_create(numerator, denominator, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    return anchor;
}

static Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration;
    assert(duration_create(numerator, denominator, &duration) == DURATION_OK);
    return duration;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *pitch;
    assert(pitch_identity_create(token, strlen(token) + 1, &pitch) == PITCH_IDENTITY_OK);
    return pitch;
}

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
}

static NoteEvent *make_note_event(const char *id_value, const char *pitch_token) {
    NoteEvent *note_event;
    assert(note_event_create(make_id(id_value), make_temporal_anchor(0, 4), make_duration(1, 4),
                              make_pitch(pitch_token), NULL, NULL, make_absent_provenance(),
                              &note_event) == NOTE_EVENT_OK);
    return note_event;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out, for both entry points. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(phrase_boundary_detection_analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(phrase_boundary_detection_signal_analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(PHRASE_BOUNDARY_DETECTION_ANALYZER.name, "PhraseBoundaryDetection") == 0);
    assert(strcmp(PHRASE_BOUNDARY_DETECTION_SIGNAL_ANALYZER.name, "PhraseBoundaryDetectionSignal") == 0);

    /* Behavioral contract: an empty point list is a valid, empty AnalysisResult, for both entry points. */
    PhraseBoundaryDetectionInput empty_input = {NULL, 0, 0};
    AnalysisResult *empty_result;
    assert(phrase_boundary_detection_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);
    assert(phrase_boundary_detection_signal_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    /* Behavioral contract: an empty point list is a valid, empty occurrence set. */
    PhraseOccurrence **empty_occurrences;
    size_t empty_occurrence_count;
    assert(phrase_boundary_detection_build_occurrences(NULL, 0, 0, &empty_occurrences, &empty_occurrence_count) ==
           PHRASE_BOUNDARY_DETECTION_OCC_OK);
    assert(empty_occurrences == NULL);
    assert(empty_occurrence_count == 0);

    /*
     * Analysis-regression sequence, max_length_without_cadence overridden to 3:
     *   p0: no cadence, no discontinuity           -- non-boundary
     *   p1: no cadence, no discontinuity            -- non-boundary
     *   p2: no cadence, discontinuity, 3rd since last -- boundary (contour/rhythm)
     *   p3: cadence "unclassified"                   -- boundary (cadence)
     *   p4: no cadence, no discontinuity              -- trailing, no boundary signal
     * Phrases: [p0,p1,p2], [p3], [p4] (trailing partial phrase).
     */
    NoteEvent *e0 = make_note_event("note-0", "C4");
    NoteEvent *e1 = make_note_event("note-1", "D4");
    NoteEvent *e2 = make_note_event("note-2", "E4");
    NoteEvent *e3 = make_note_event("note-3", "F4");
    NoteEvent *e4 = make_note_event("note-4", "G4");

    PhraseBoundaryDetectionPoint points[5] = {
        {e0, false, NULL, false},
        {e1, false, NULL, false},
        {e2, false, NULL, true},
        {e3, true, cadence_type_label(CADENCE_TYPE_UNKNOWN), false},
        {e4, false, NULL, false},
    };
    PhraseBoundaryDetectionInput input = {points, 5, 3};

    /* Delegated plain-boundary Interpretation matches PhraseSegmentation's output shape exactly. */
    AnalysisResult *plain_result;
    assert(phrase_boundary_detection_analyze(&input, NULL, NULL, &plain_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(plain_result) == 5);
    static const bool expected_is_boundary[5] = {false, false, true, true, false};
    for (size_t i = 0; i < 5; i++) {
        const Interpretation *interpretation = analysis_result_interpretation_at(plain_result, i);
        size_t claim_size;
        const PhraseSegmentationClaim *claim =
            (const PhraseSegmentationClaim *)value_content(interpretation_claim(interpretation), &claim_size);
        assert(claim_size == sizeof(PhraseSegmentationClaim));
        assert(claim->is_boundary == expected_is_boundary[i]);
    }
    analysis_result_destroy(plain_result);

    /* Signal-recording Interpretation: exactly the two boundary points, with the right signal/cadence_type/confidence. */
    AnalysisResult *signal_result;
    assert(phrase_boundary_detection_signal_analyze(&input, NULL, NULL, &signal_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(signal_result) == 2);

    const Interpretation *first_signal = analysis_result_interpretation_at(signal_result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(first_signal)), "note-2") == 0);
    size_t first_claim_size;
    const PhraseBoundaryDetectionSignalClaim *first_claim = (const PhraseBoundaryDetectionSignalClaim *)value_content(
        interpretation_claim(first_signal), &first_claim_size);
    assert(first_claim_size == sizeof(PhraseBoundaryDetectionSignalClaim));
    assert(first_claim->signal == PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM);
    assert(strcmp(first_claim->cadence_type, "") == 0);
    const Uncertainty *first_uncertainty = interpretation_uncertainty(first_signal);
    assert(uncertainty_scalar_confidence_numerator(first_uncertainty) ==
           PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_NUM);
    assert(uncertainty_scalar_confidence_denominator(first_uncertainty) ==
           PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_DEN);

    const Interpretation *second_signal = analysis_result_interpretation_at(signal_result, 1);
    assert(strcmp(entity_id_value(interpretation_target_id(second_signal)), "note-3") == 0);
    size_t second_claim_size;
    const PhraseBoundaryDetectionSignalClaim *second_claim =
        (const PhraseBoundaryDetectionSignalClaim *)value_content(interpretation_claim(second_signal),
                                                                    &second_claim_size);
    assert(second_claim_size == sizeof(PhraseBoundaryDetectionSignalClaim));
    assert(second_claim->signal == PHRASE_BOUNDARY_SIGNAL_CADENCE);
    assert(strcmp(second_claim->cadence_type, cadence_type_label(CADENCE_TYPE_UNKNOWN)) == 0);
    const Uncertainty *second_uncertainty = interpretation_uncertainty(second_signal);
    assert(uncertainty_scalar_confidence_numerator(second_uncertainty) == 1);
    assert(uncertainty_scalar_confidence_denominator(second_uncertainty) == 1);

    analysis_result_destroy(signal_result);

    /* PhraseOccurrence grouping: [p0,p1,p2], [p3], [p4] (trailing partial phrase). */
    PhraseOccurrence **occurrences;
    size_t occurrence_count;
    assert(phrase_boundary_detection_build_occurrences(points, 5, 3, &occurrences, &occurrence_count) ==
           PHRASE_BOUNDARY_DETECTION_OCC_OK);
    assert(occurrence_count == 3);

    assert(phrase_occurrence_region_event_id_count(occurrences[0]) == 3);
    assert(strcmp(entity_id_value(phrase_occurrence_region_event_id_at(occurrences[0], 0)), "note-0") == 0);
    assert(strcmp(entity_id_value(phrase_occurrence_region_event_id_at(occurrences[0], 1)), "note-1") == 0);
    assert(strcmp(entity_id_value(phrase_occurrence_region_event_id_at(occurrences[0], 2)), "note-2") == 0);

    assert(phrase_occurrence_region_event_id_count(occurrences[1]) == 1);
    assert(strcmp(entity_id_value(phrase_occurrence_region_event_id_at(occurrences[1], 0)), "note-3") == 0);

    assert(phrase_occurrence_region_event_id_count(occurrences[2]) == 1);
    assert(strcmp(entity_id_value(phrase_occurrence_region_event_id_at(occurrences[2], 0)), "note-4") == 0);

    phrase_boundary_detection_destroy_occurrences(occurrences, occurrence_count);

    note_event_destroy(e0);
    note_event_destroy(e1);
    note_event_destroy(e2);
    note_event_destroy(e3);
    note_event_destroy(e4);

    printf("test_phrase_boundary_detection: ok\n");
    return 0;
}
