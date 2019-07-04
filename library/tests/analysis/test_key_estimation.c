#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/key_estimation.h"
#include "core/entity.h"
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
 * Behavioral-contract test: KeyEstimation conforms to the Analyzer
 * interface (analysis/analyzer.h) -- NULL input, a NULL region, a NULL
 * framework, and an empty candidate set are all rejected without
 * touching *out -- and its output carries the Interpretation/Evidence/
 * Uncertainty architecture TDD Sec242 requires: a claim naming the
 * best-fit candidate key, an Uncertainty holding the full candidate set
 * (key estimation is ambiguous, same as scale/chord identification),
 * and a referenced Evidence whose source names the Framework the
 * estimate was made under.
 *
 * Analysis-regression test: a fixed two-candidate set ("C major" 3/4,
 * "A natural minor" 1/4) under a fixed Framework
 * ("common-practice-tonal-harmony") always yields the same top pick,
 * the same Evidence strength/source, and the same full candidate set
 * on the Interpretation's Uncertainty -- pinning KeyEstimation's
 * behavior against a future refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Structure *make_region(const char *id_value, const char *event_id_value) {
    SchemaVersion *schema_version;
    assert(schema_version_create(1, &schema_version) == SCHEMA_VERSION_OK);
    Entity *entity;
    assert(entity_create(make_id(id_value), "Structure", schema_version, NULL, &entity) == ENTITY_OK);

    EntityID *event_ids[1] = {make_id(event_id_value)};
    Structure *structure;
    assert(structure_create(entity, event_ids, 1, &structure) == STRUCTURE_OK);
    return structure;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(KEY_ESTIMATION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(KEY_ESTIMATION_ANALYZER.name, "KeyEstimation") == 0);

    Structure *region = make_region("structure-section-1", "note-c4");
    Framework *framework;
    assert(framework_create(make_id("framework-common-practice-tonal-harmony"),
                             "common-practice-tonal-harmony", &framework) == FRAMEWORK_OK);

    UncertaintyCandidate candidates[2] = {
        {"C major", 3, 4},
        {"A natural minor", 1, 4},
    };

    /* Behavioral contract: a NULL region is rejected. */
    KeyEstimationInput no_region_input = {NULL, framework, candidates, 2};
    AnalysisResult *no_region_result;
    assert(key_estimation_analyze(&no_region_input, NULL, NULL, &no_region_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: a NULL framework is rejected. */
    KeyEstimationInput no_framework_input = {region, NULL, candidates, 2};
    AnalysisResult *no_framework_result;
    assert(key_estimation_analyze(&no_framework_input, NULL, NULL, &no_framework_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: an empty candidate set is rejected. */
    KeyEstimationInput no_candidates_input = {region, framework, NULL, 0};
    AnalysisResult *no_candidates_result;
    assert(key_estimation_analyze(&no_candidates_input, NULL, NULL, &no_candidates_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Analysis-regression: "C major" (3/4) beats "A natural minor" (1/4). */
    KeyEstimationInput input = {region, framework, candidates, 2};

    AnalysisResult *result;
    assert(KEY_ESTIMATION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "structure-section-1") == 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);

    size_t claim_size;
    const char *claim_content = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == strlen("C major") + 1);
    assert(strcmp(claim_content, "C major") == 0);

    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_CANDIDATE_SET);
    assert(uncertainty_candidate_count(uncertainty) == 2);
    assert(strcmp(uncertainty_candidate_label_at(uncertainty, 0), "C major") == 0);
    assert(uncertainty_candidate_weight_numerator_at(uncertainty, 0) == 3);
    assert(uncertainty_candidate_weight_denominator_at(uncertainty, 0) == 4);
    assert(strcmp(uncertainty_candidate_label_at(uncertainty, 1), "A natural minor") == 0);

    const Evidence *evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(evidence), "framework-relative key estimation") == 0);
    assert(strcmp(evidence_source(evidence), "common-practice-tonal-harmony") == 0);
    assert(evidence_strength_numerator(evidence) == 3);
    assert(evidence_strength_denominator(evidence) == 4);

    analysis_result_destroy(result);
    framework_destroy(framework);
    structure_destroy(region);

    printf("test_key_estimation: ok\n");
    return 0;
}