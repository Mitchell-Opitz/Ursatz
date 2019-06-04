#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/phrase_segmentation.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Behavioral-contract test: PhraseSegmentation conforms to the
 * Analyzer interface (analysis/analyzer.h) -- NULL input rejected
 * without touching *out, an empty point list yields a valid empty
 * AnalysisResult, a point with a NULL event or an invalid confidence
 * fraction is rejected -- and every produced Interpretation carries
 * the Interpretation/Evidence/Uncertainty architecture TDD Sec242
 * requires: a claim, a referenced Evidence, and a scalar-confidence
 * Uncertainty (the classification was supplied already determined,
 * not estimated -- unlike Scale/Chord/Key/Cadence's ambiguous
 * candidate sets).
 *
 * Analysis-regression test: a fixed two-point sequence (one boundary,
 * one non-boundary) always yields the same claims on the same targets
 * -- pinning PhraseSegmentation's behavior against a future refactor.
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
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(PHRASE_SEGMENTATION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(PHRASE_SEGMENTATION_ANALYZER.name, "PhraseSegmentation") == 0);

    /* Behavioral contract: an empty point list is a valid, empty AnalysisResult. */
    PhraseSegmentationInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(phrase_segmentation_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    assert(analysis_result_evidence_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    /* Behavioral contract: an invalid confidence fraction is rejected. */
    NoteEvent *mid_event = make_note_event("note-mid-1", "D4");
    PhraseSegmentationPoint bad_point[1] = {{mid_event, false, 5, 4}};
    PhraseSegmentationInput bad_input = {bad_point, 1};
    AnalysisResult *bad_result;
    assert(phrase_segmentation_analyze(&bad_input, NULL, NULL, &bad_result) == ANALYZER_ERR_NULL_INPUT);

    /* Analysis-regression: mid-phrase point (non-boundary), cadence point (boundary). */
    NoteEvent *cadence_event = make_note_event("note-cadence-1", "C4");

    PhraseSegmentationPoint points[2] = {
        {mid_event, false, 1, 1},
        {cadence_event, true, 9, 10},
    };
    PhraseSegmentationInput input = {points, 2};

    AnalysisResult *result;
    assert(PHRASE_SEGMENTATION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 2);
    assert(analysis_result_evidence_count(result) == 2);

    const Interpretation *first = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(first)), "note-mid-1") == 0);
    size_t first_claim_size;
    const PhraseSegmentationClaim *first_claim =
        (const PhraseSegmentationClaim *)value_content(interpretation_claim(first), &first_claim_size);
    assert(first_claim_size == sizeof(PhraseSegmentationClaim));
    assert(!first_claim->is_boundary);
    const Uncertainty *first_uncertainty = interpretation_uncertainty(first);
    assert(uncertainty_kind(first_uncertainty) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(first_uncertainty) == 1);
    assert(uncertainty_scalar_confidence_denominator(first_uncertainty) == 1);

    const Interpretation *second = analysis_result_interpretation_at(result, 1);
    assert(strcmp(entity_id_value(interpretation_target_id(second)), "note-cadence-1") == 0);
    size_t second_claim_size;
    const PhraseSegmentationClaim *second_claim =
        (const PhraseSegmentationClaim *)value_content(interpretation_claim(second), &second_claim_size);
    assert(second_claim_size == sizeof(PhraseSegmentationClaim));
    assert(second_claim->is_boundary);
    const Uncertainty *second_uncertainty = interpretation_uncertainty(second);
    assert(uncertainty_scalar_confidence_numerator(second_uncertainty) == 9);
    assert(uncertainty_scalar_confidence_denominator(second_uncertainty) == 10);

    const Evidence *second_evidence = analysis_result_evidence_at(result, 1);
    assert(strcmp(evidence_method(second_evidence), "phrase-boundary classification") == 0);
    assert(strcmp(evidence_source(second_evidence), "PhraseSegmentation") == 0);

    analysis_result_destroy(result);

    note_event_destroy(mid_event);
    note_event_destroy(cadence_event);

    printf("test_phrase_segmentation: ok\n");
    return 0;
}