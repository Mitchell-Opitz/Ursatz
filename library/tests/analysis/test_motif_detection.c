#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motif_detection.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "structure/motif.h"

/*
 * Behavioral-contract test: MotifDetection conforms to the Analyzer
 * interface (analysis/analyzer.h) -- NULL input, a NULL motif, an
 * occurrence with zero matched events, and an occurrence with a NULL
 * matched-event entry are all rejected without touching *out -- and
 * every produced Interpretation carries the Interpretation/Evidence/
 * Uncertainty architecture TDD Sec242 requires. MotifDetection never
 * duplicates a NoteEvent (TDD Sec152): the analyzer only ever copies
 * EntityID value strings (through motif_occurrence_create's own
 * validation), never event content.
 *
 * Analysis-regression test: a fixed motif with a fixed single
 * three-event occurrence at confidence 3/4 always yields the same
 * claim (matched_event_id_count == 3) and the same scalar-confidence
 * Uncertainty -- pinning MotifDetection's behavior against a future
 * refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Motif *make_motif(const char *id_value, const char *event_id_value) {
    EntityID *event_ids[1] = {make_id(event_id_value)};
    Motif *motif;
    assert(motif_create(make_id(id_value), event_ids, 1, &motif) == MOTIF_OK);
    return motif;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(MOTIF_DETECTION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(MOTIF_DETECTION_ANALYZER.name, "MotifDetection") == 0);

    Motif *motif = make_motif("motif-turn-figure", "note-motif-def-1");

    /* Behavioral contract: a NULL motif is rejected. */
    MotifDetectionInput no_motif_input = {NULL, NULL, 0};
    AnalysisResult *no_motif_result;
    assert(motif_detection_analyze(&no_motif_input, NULL, NULL, &no_motif_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: an empty occurrence list is a valid, empty AnalysisResult. */
    MotifDetectionInput empty_input = {motif, NULL, 0};
    AnalysisResult *empty_result;
    assert(motif_detection_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    /* Behavioral contract: zero matched events is rejected. */
    MotifDetectionOccurrence empty_occurrence = {NULL, 0, 1, 1};
    MotifDetectionInput zero_events_input = {motif, &empty_occurrence, 1};
    AnalysisResult *zero_events_result;
    assert(motif_detection_analyze(&zero_events_input, NULL, NULL, &zero_events_result) == ANALYZER_ERR_NULL_INPUT);

    /* Analysis-regression: one occurrence matching 3 events, confidence 3/4. */
    EntityID *event_a = make_id("note-melody-5");
    EntityID *event_b = make_id("note-melody-6");
    EntityID *event_c = make_id("note-melody-7");
    const EntityID *matched_event_ids[3] = {event_a, event_b, event_c};

    MotifDetectionOccurrence occurrence = {matched_event_ids, 3, 3, 4};
    MotifDetectionInput input = {motif, &occurrence, 1};

    AnalysisResult *result;
    assert(MOTIF_DETECTION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);
    /* The target is the validated MotifOccurrence's own id, distinct from the motif's and every matched event's id. */
    const char *target_value = entity_id_value(interpretation_target_id(interpretation));
    assert(strcmp(target_value, "motif-turn-figure") != 0);
    assert(strcmp(target_value, "note-melody-5") != 0);

    size_t claim_size;
    const MotifDetectionClaim *claim =
        (const MotifDetectionClaim *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == sizeof(MotifDetectionClaim));
    assert(claim->matched_event_id_count == 3);

    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 3);
    assert(uncertainty_scalar_confidence_denominator(uncertainty) == 4);

    const Evidence *evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(evidence), "motif-occurrence pattern matching") == 0);
    assert(strcmp(evidence_source(evidence), "motif-turn-figure") == 0);
    assert(evidence_strength_numerator(evidence) == 3);
    assert(evidence_strength_denominator(evidence) == 4);

    analysis_result_destroy(result);

    entity_id_destroy(event_a);
    entity_id_destroy(event_b);
    entity_id_destroy(event_c);
    motif_destroy(motif);

    printf("test_motif_detection: ok\n");
    return 0;
}