#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/interval_analysis.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Behavioral-contract test: IntervalAnalysis conforms to the Analyzer
 * interface (analysis/analyzer.h) -- NULL input rejected without
 * touching *out, an empty pair list yields a valid empty AnalysisResult
 * -- and every produced Interpretation carries the Interpretation/
 * Evidence/Uncertainty architecture TDD Sec242 requires: a claim, a
 * referenced Evidence, and an attached Uncertainty, never a bare
 * conclusion.
 *
 * Analysis-regression test: a fixed three-note line (C4, E4, G4) with
 * fixed measured displacements (+4, +3 semitones) always yields the
 * same two Interpretations in order, each with the expected target,
 * claim, and backing Evidence -- pinning IntervalAnalysis's behavior
 * against a future refactor.
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

static NoteEvent *make_note_event(const char *id_value, long position_numerator, const char *pitch_token) {
    NoteEvent *note_event;
    assert(note_event_create(make_id(id_value), make_temporal_anchor(position_numerator, 4), make_duration(1, 4),
                              make_pitch(pitch_token), NULL, NULL, make_absent_provenance(),
                              &note_event) == NOTE_EVENT_OK);
    return note_event;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(INTERVAL_ANALYSIS_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(INTERVAL_ANALYSIS_ANALYZER.name, "IntervalAnalysis") == 0);

    /* Behavioral contract: an empty pair list is a valid, empty AnalysisResult. */
    IntervalAnalysisInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(interval_analysis_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    assert(analysis_result_evidence_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    /* Analysis-regression: C4 -> E4 (+4 semitones) -> G4 (+3 semitones). */
    NoteEvent *c4 = make_note_event("note-c4", 0, "C4");
    NoteEvent *e4 = make_note_event("note-e4", 1, "E4");
    NoteEvent *g4 = make_note_event("note-g4", 2, "G4");

    IntervalAnalysisPair pairs[2] = {
        {c4, e4, 4},
        {e4, g4, 3},
    };
    IntervalAnalysisInput input = {pairs, 2};

    AnalysisResult *result;
    assert(INTERVAL_ANALYSIS_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 2);
    assert(analysis_result_evidence_count(result) == 2);

    /* First pair: C4 -> E4, +4 semitones. */
    const Interpretation *first = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(first)), "note-e4") == 0);
    assert(interpretation_status(first) == INTERPRETATION_STATUS_MACHINE_GENERATED);
    size_t first_claim_size;
    const long *first_claim = (const long *)value_content(interpretation_claim(first), &first_claim_size);
    assert(first_claim_size == sizeof(long));
    assert(*first_claim == 4);
    assert(uncertainty_kind(interpretation_uncertainty(first)) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(first)) == 1);
    assert(uncertainty_scalar_confidence_denominator(interpretation_uncertainty(first)) == 1);
    assert(interpretation_evidence_id(first) != NULL);

    const Evidence *first_evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(first_evidence), "pairwise directed semitone difference") == 0);
    assert(strcmp(evidence_source(first_evidence), "IntervalAnalysis") == 0);
    assert(strcmp(entity_id_value(interpretation_evidence_id(first)), entity_id_value(evidence_id(first_evidence))) ==
           0);

    /* Second pair: E4 -> G4, +3 semitones. */
    const Interpretation *second = analysis_result_interpretation_at(result, 1);
    assert(strcmp(entity_id_value(interpretation_target_id(second)), "note-g4") == 0);
    size_t second_claim_size;
    const long *second_claim = (const long *)value_content(interpretation_claim(second), &second_claim_size);
    assert(second_claim_size == sizeof(long));
    assert(*second_claim == 3);

    analysis_result_destroy(result);

    /* Behavioral contract: a pair with a NULL event is rejected. */
    IntervalAnalysisPair bad_pairs[1] = {{c4, NULL, 0}};
    IntervalAnalysisInput bad_input = {bad_pairs, 1};
    AnalysisResult *bad_result;
    assert(interval_analysis_analyze(&bad_input, NULL, NULL, &bad_result) == ANALYZER_ERR_NULL_INPUT);

    note_event_destroy(c4);
    note_event_destroy(e4);
    note_event_destroy(g4);

    printf("test_interval_analysis: ok\n");
    return 0;
}