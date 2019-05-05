#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/analyzer.h"
#include "context/context.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

/*
 * Interface-contract test: a concrete Analyzer is a name plus an
 * AnalyzerFn, invoked as analyze(input, context, config) -> out (TDD
 * Registry #12). This mock analyzer stands in for a real one (Interval
 * Analysis etc. arrive in later commits) purely to exercise the
 * interface's shape: input/context/config are borrowed (never
 * destroyed by the analyzer), a NULL input is rejected without
 * touching *out, and a successful call hands back an AnalysisResult
 * that OWNS the Interpretation and Evidence it derived -- there is no
 * store elsewhere for those objects to live.
 *
 * Analysis-regression test: a fixed input ("C4-E4-G4") always produces
 * the same single Interpretation, backed by the same single Evidence --
 * pinning the mock analyzer's behavior so a future refactor of the
 * interface plumbing cannot silently change what a caller receives.
 */

static AnalyzerResult mock_analyze(const void *input, const Context *context, const Value *config,
                                    AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    EntityID *evidence_id;
    if (entity_id_create("evidence-mock-1", &evidence_id) != ENTITY_ID_OK) {
        return ANALYZER_ERR_ALLOC;
    }
    Evidence *evidence;
    if (evidence_create(evidence_id, 3, 4, 1, 1, 1, 1, false, "mock-method", "mock-source", &evidence) !=
        EVIDENCE_OK) {
        entity_id_destroy(evidence_id);
        return ANALYZER_ERR_ALLOC;
    }

    EntityID *interpretation_id;
    assert(entity_id_create("interpretation-mock-1", &interpretation_id) == ENTITY_ID_OK);
    EntityID *target_id;
    assert(entity_id_create("note-event-mock-1", &target_id) == ENTITY_ID_OK);
    EntityID *evidence_ref_id;
    assert(entity_id_create("evidence-mock-1", &evidence_ref_id) == ENTITY_ID_OK);

    long claim_content = 4;
    Value *claim;
    assert(value_create("directed-semitones", &claim_content, sizeof(claim_content), &claim) == VALUE_OK);

    Uncertainty *uncertainty;
    assert(uncertainty_create_unknown(&uncertainty) == UNCERTAINTY_OK);

    ProvenanceReference *interpretation_provenance;
    assert(provenance_reference_create_absent(&interpretation_provenance) == PROVENANCE_REFERENCE_OK);

    Interpretation *interpretation;
    if (interpretation_create(interpretation_id, target_id, claim, NULL, evidence_ref_id, uncertainty,
                               "mock-analyzer", "mock-method", INTERPRETATION_STATUS_MACHINE_GENERATED,
                               interpretation_provenance, &interpretation) != INTERPRETATION_OK) {
        evidence_destroy(evidence);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    Interpretation *interpretations[1] = {interpretation};
    Evidence *evidences[1] = {evidence};

    EntityID *result_id;
    assert(entity_id_create("analysis-result-mock-1", &result_id) == ENTITY_ID_OK);
    ProvenanceReference *result_provenance;
    assert(provenance_reference_create_absent(&result_provenance) == PROVENANCE_REFERENCE_OK);

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, 1, evidences, 1, result_provenance, &result) !=
        ANALYSIS_RESULT_OK) {
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    *out = result;
    return ANALYZER_OK;
}

int main(void) {
    /* AnalysisResult: construction, accessors, purity of an empty result. */
    EntityID *empty_id;
    assert(entity_id_create("analysis-result-empty", &empty_id) == ENTITY_ID_OK);
    ProvenanceReference *empty_provenance;
    assert(provenance_reference_create_absent(&empty_provenance) == PROVENANCE_REFERENCE_OK);

    AnalysisResult *empty_result;
    assert(analysis_result_create(empty_id, NULL, 0, NULL, 0, empty_provenance, &empty_result) ==
           ANALYSIS_RESULT_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    assert(analysis_result_evidence_count(empty_result) == 0);
    assert(provenance_reference_state(analysis_result_provenance(empty_result)) == PROVENANCE_REFERENCE_ABSENT);
    analysis_result_destroy(empty_result);

    /* AnalysisResult rejects a NULL entry in a non-empty interpretation list. */
    EntityID *rejected_id;
    assert(entity_id_create("analysis-result-rejected", &rejected_id) == ENTITY_ID_OK);
    ProvenanceReference *rejected_provenance;
    assert(provenance_reference_create_absent(&rejected_provenance) == PROVENANCE_REFERENCE_OK);
    Interpretation *bad_interpretations[1] = {NULL};
    AnalysisResult *rejected_result;
    assert(analysis_result_create(rejected_id, bad_interpretations, 1, NULL, 0, rejected_provenance,
                                   &rejected_result) == ANALYSIS_RESULT_ERR_NULL_INTERPRETATION);
    entity_id_destroy(rejected_id);
    provenance_reference_destroy(rejected_provenance);

    /* Analyzer interface contract: a concrete analyzer is name + AnalyzerFn. */
    Analyzer mock = {"mock-analyzer", mock_analyze};
    assert(strcmp(mock.name, "mock-analyzer") == 0);

    /* NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(mock.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);

    /* Analysis-regression: fixed input always yields the same Interpretation, backed by the same Evidence. */
    const char *input = "C4-E4-G4";
    AnalysisResult *result;
    assert(mock.analyze(input, NULL, NULL, &result) == ANALYZER_OK);

    assert(analysis_result_interpretation_count(result) == 1);
    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "note-event-mock-1") == 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);
    size_t claim_size;
    const long *claim_content = (const long *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == sizeof(long));
    assert(*claim_content == 4);

    assert(analysis_result_evidence_count(result) == 1);
    const Evidence *evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(evidence), "mock-method") == 0);
    assert(strcmp(evidence_source(evidence), "mock-source") == 0);
    assert(evidence_strength_numerator(evidence) == 3);
    assert(evidence_strength_denominator(evidence) == 4);

    analysis_result_destroy(result);

    printf("test_analyzer: ok\n");
    return 0;
}