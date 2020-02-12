#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/cadence_detection.h"
#include "core/entity.h"
#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "structure/structure.h"
#include "theory/framework.h"

/*
 * Behavioral-contract test: CadenceDetection conforms to the Analyzer
 * interface (analysis/analyzer.h) -- NULL input, a NULL region, and an
 * empty candidate set are all rejected without touching *out -- and
 * its output carries the Interpretation/Evidence/Uncertainty
 * architecture TDD Sec242 requires: a claim naming the best-fit
 * cadence type, an Uncertainty holding the full candidate set (cadence
 * type identification is ambiguous, same as scale/chord/key
 * identification), and a referenced Evidence.
 *
 * Analysis-regression test: a fixed two-candidate set ("authentic
 * (PAC)" 3/4, "deceptive" 1/4) always yields the same top pick, the
 * same Evidence strength, and the same full candidate set on the
 * Interpretation's Uncertainty -- pinning CadenceDetection's behavior
 * against a future refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Structure *make_region(const char *id_value, const char *event_id_value) {
    SchemaVersion *schema_version;
    assert(schema_version_create(1, &schema_version) == SCHEMA_VERSION_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);

    Entity *entity;
    assert(entity_create(make_id(id_value), "Structure", schema_version, NULL, provenance, &entity) == ENTITY_OK);

    EntityID *event_ids[1] = {make_id(event_id_value)};
    Structure *structure;
    assert(structure_create(entity, event_ids, 1, &structure) == STRUCTURE_OK);
    return structure;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(CADENCE_DETECTION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(CADENCE_DETECTION_ANALYZER.name, "CadenceDetection") == 0);

    Structure *region = make_region("structure-phrase-end-1", "note-g4");

    /* Behavioral contract: a NULL region is rejected. */
    CadenceDetectionInput no_region_input = {NULL, NULL, 0};
    AnalysisResult *no_region_result;
    assert(cadence_detection_analyze(&no_region_input, NULL, NULL, &no_region_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: an empty candidate set is rejected. */
    CadenceDetectionInput no_candidates_input = {region, NULL, 0};
    AnalysisResult *no_candidates_result;
    assert(cadence_detection_analyze(&no_candidates_input, NULL, NULL, &no_candidates_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Analysis-regression: "authentic (PAC)" (3/4) beats "deceptive" (1/4). */
    UncertaintyCandidate candidates[2] = {
        {"authentic (PAC)", 3, 4},
        {"deceptive", 1, 4},
    };
    CadenceDetectionInput input = {region, candidates, 2, NULL};

    AnalysisResult *result;
    assert(CADENCE_DETECTION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "structure-phrase-end-1") == 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);

    size_t claim_size;
    const char *claim_content = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == strlen("authentic (PAC)") + 1);
    assert(strcmp(claim_content, "authentic (PAC)") == 0);

    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_CANDIDATE_SET);
    assert(uncertainty_candidate_count(uncertainty) == 2);
    assert(strcmp(uncertainty_candidate_label_at(uncertainty, 0), "authentic (PAC)") == 0);
    assert(uncertainty_candidate_weight_numerator_at(uncertainty, 0) == 3);
    assert(uncertainty_candidate_weight_denominator_at(uncertainty, 0) == 4);
    assert(strcmp(uncertainty_candidate_label_at(uncertainty, 1), "deceptive") == 0);

    const Evidence *evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(evidence), "candidate-cadence-type matching") == 0);
    assert(strcmp(evidence_source(evidence), "CadenceDetection") == 0);
    assert(evidence_strength_numerator(evidence) == 3);
    assert(evidence_strength_denominator(evidence) == 4);

    assert(interpretation_framework_reference(interpretation) == NULL);

    analysis_result_destroy(result);

    /* Regression: when a Framework is supplied, framework_reference names it. */
    Framework *framework;
    assert(framework_create(make_id("framework-common-practice-minimal"), "common-practice-minimal", &framework) ==
           FRAMEWORK_OK);
    CadenceDetectionInput framework_input = {region, candidates, 2, framework};
    AnalysisResult *framework_result;
    assert(CADENCE_DETECTION_ANALYZER.analyze(&framework_input, NULL, NULL, &framework_result) == ANALYZER_OK);
    const Interpretation *framework_interpretation = analysis_result_interpretation_at(framework_result, 0);
    const FrameworkReference *framework_reference = interpretation_framework_reference(framework_interpretation);
    assert(framework_reference != NULL);
    assert(strcmp(entity_id_value(framework_reference_target_id(framework_reference)),
                   "framework-common-practice-minimal") == 0);

    analysis_result_destroy(framework_result);
    framework_destroy(framework);
    structure_destroy(region);

    printf("test_cadence_detection: ok\n");
    return 0;
}