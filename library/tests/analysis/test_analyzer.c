#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/analyzer.h"
#include "context/context.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"

/*
 * Interface-contract test: a concrete Analyzer is a name plus an
 * AnalyzerFn, invoked as analyze(input, context, config) -> out (TDD
 * Registry #12). This mock analyzer stands in for a real one (Interval
 * Analysis etc. arrive in later commits) purely to exercise the
 * interface's shape: input/context/config are borrowed (never
 * destroyed by the analyzer), a NULL input is rejected without
 * touching *out, and a successful call hands back an AnalysisResult
 * the caller owns.
 *
 * Analysis-regression test: a fixed input ("C4-E4-G4") always produces
 * the same single Interpretation reference -- pinning the mock
 * analyzer's behavior so a future refactor of the interface plumbing
 * cannot silently change what a caller receives.
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

    EntityID *result_id;
    if (entity_id_create("analysis-result-mock-1", &result_id) != ENTITY_ID_OK) {
        return ANALYZER_ERR_ALLOC;
    }

    EntityID *interpretation_id;
    if (entity_id_create("interpretation-mock-1", &interpretation_id) != ENTITY_ID_OK) {
        entity_id_destroy(result_id);
        return ANALYZER_ERR_ALLOC;
    }

    EntityID *interpretation_ids[1] = {interpretation_id};

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(interpretation_id);
        entity_id_destroy(result_id);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretation_ids, 1, provenance, &result) != ANALYSIS_RESULT_OK) {
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
    assert(analysis_result_create(empty_id, NULL, 0, empty_provenance, &empty_result) == ANALYSIS_RESULT_OK);
    assert(analysis_result_interpretation_id_count(empty_result) == 0);
    assert(provenance_reference_state(analysis_result_provenance(empty_result)) == PROVENANCE_REFERENCE_ABSENT);
    analysis_result_destroy(empty_result);

    /* AnalysisResult rejects a NULL entry in a non-empty id list. */
    EntityID *rejected_id;
    assert(entity_id_create("analysis-result-rejected", &rejected_id) == ENTITY_ID_OK);
    ProvenanceReference *rejected_provenance;
    assert(provenance_reference_create_absent(&rejected_provenance) == PROVENANCE_REFERENCE_OK);
    EntityID *bad_ids[1] = {NULL};
    AnalysisResult *rejected_result;
    assert(analysis_result_create(rejected_id, bad_ids, 1, rejected_provenance, &rejected_result) ==
           ANALYSIS_RESULT_ERR_NULL_INTERPRETATION_ID);
    entity_id_destroy(rejected_id);
    provenance_reference_destroy(rejected_provenance);

    /* Analyzer interface contract: a concrete analyzer is name + AnalyzerFn. */
    Analyzer mock = {"mock-analyzer", mock_analyze};
    assert(strcmp(mock.name, "mock-analyzer") == 0);

    /* NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(mock.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);

    /* Analysis-regression: fixed input always yields the same single Interpretation reference. */
    const char *input = "C4-E4-G4";
    AnalysisResult *result;
    assert(mock.analyze(input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_id_count(result) == 1);
    const EntityID *interpretation_id = analysis_result_interpretation_id_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_id), "interpretation-mock-1") == 0);
    assert(provenance_reference_state(analysis_result_provenance(result)) == PROVENANCE_REFERENCE_ABSENT);
    analysis_result_destroy(result);

    printf("test_analyzer: ok\n");
    return 0;
}