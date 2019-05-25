#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/voice_leading_analysis.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Behavioral-contract test: VoiceLeadingAnalysis conforms to the
 * Analyzer interface (analysis/analyzer.h) -- NULL input rejected
 * without touching *out, an empty pair list yields a valid empty
 * AnalysisResult, a pair with a NULL event is rejected -- and every
 * produced Interpretation carries the Interpretation/Evidence/
 * Uncertainty architecture TDD Sec242 requires: a claim, a referenced
 * Evidence, and a scalar-confidence Uncertainty (the classification was
 * supplied already determined, not estimated -- unlike Scale/Chord/
 * Key's ambiguous candidate sets).
 *
 * Analysis-regression test: a fixed pair classified as parallel motion
 * into a perfect interval (the textbook "parallel fifths" shape)
 * always yields the same claim on the same target -- pinning
 * VoiceLeadingAnalysis's behavior against a future refactor.
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
    assert(VOICE_LEADING_ANALYSIS_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(VOICE_LEADING_ANALYSIS_ANALYZER.name, "VoiceLeadingAnalysis") == 0);

    /* Behavioral contract: an empty pair list is a valid, empty AnalysisResult. */
    VoiceLeadingAnalysisInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(voice_leading_analysis_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    assert(analysis_result_evidence_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    /* Analysis-regression: soprano and alto move in parallel into a perfect fifth. */
    NoteEvent *soprano_to = make_note_event("note-soprano-2", "D5");
    NoteEvent *alto_to = make_note_event("note-alto-2", "G4");

    VoiceLeadingAnalysisPair pairs[1] = {
        {soprano_to, alto_to, VOICE_LEADING_MOTION_PARALLEL, true},
    };
    VoiceLeadingAnalysisInput input = {pairs, 1};

    AnalysisResult *result;
    assert(VOICE_LEADING_ANALYSIS_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "note-soprano-2") == 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);

    size_t claim_size;
    const VoiceLeadingClaim *claim =
        (const VoiceLeadingClaim *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == sizeof(VoiceLeadingClaim));
    assert(claim->motion == VOICE_LEADING_MOTION_PARALLEL);
    assert(claim->resulting_interval_is_perfect);

    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 1);
    assert(uncertainty_scalar_confidence_denominator(uncertainty) == 1);

    const Evidence *evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(evidence), "voice-pair motion classification") == 0);
    assert(strcmp(evidence_source(evidence), "VoiceLeadingAnalysis") == 0);

    analysis_result_destroy(result);

    /* Behavioral contract: a pair with a NULL event is rejected. */
    VoiceLeadingAnalysisPair bad_pairs[1] = {{soprano_to, NULL, VOICE_LEADING_MOTION_OBLIQUE, false}};
    VoiceLeadingAnalysisInput bad_input = {bad_pairs, 1};
    AnalysisResult *bad_result;
    assert(voice_leading_analysis_analyze(&bad_input, NULL, NULL, &bad_result) == ANALYZER_ERR_NULL_INPUT);

    note_event_destroy(soprano_to);
    note_event_destroy(alto_to);

    printf("test_voice_leading_analysis: ok\n");
    return 0;
}