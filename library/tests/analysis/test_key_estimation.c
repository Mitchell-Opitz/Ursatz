#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/key_estimation.h"
#include "core/entity.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "interpretation/interpretation.h"
#include "structure/structure.h"
#include "theory/framework.h"

/*
 * Behavioral-contract test: KeyEstimation conforms to the Analyzer
 * interface (analysis/analyzer.h) -- NULL input, a NULL region, a NULL
 * framework, a missing tonic name, and an empty pitch-class set are all
 * rejected without touching *out.
 *
 * Identification tests: a real major-key and a real minor-key pitch
 * collection each resolve to a real Key (via frameworks/common-
 * practice-minimal/key_identification.h) carried as this analyzer's
 * Interpretation. A collection matching neither reports
 * ANALYZER_ERR_ANALYSIS_FAILED rather than a forced guess.
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
    assert(KEY_ESTIMATION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(KEY_ESTIMATION_ANALYZER.name, "KeyEstimation") == 0);

    Framework *framework;
    assert(framework_create(make_id("framework-common-practice-minimal"), "common-practice-minimal-v1", &framework) ==
           FRAMEWORK_OK);

    int c_major[] = {0, 2, 4, 5, 7, 9, 11};

    Structure *no_region_check_region = make_region("structure-unused", "note-unused");

    /* Behavioral contract: a NULL region is rejected. */
    KeyEstimationInput no_region_input = {NULL, framework, "C", c_major, 7};
    AnalysisResult *no_region_result;
    assert(key_estimation_analyze(&no_region_input, NULL, NULL, &no_region_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: a NULL framework is rejected. */
    KeyEstimationInput no_framework_input = {no_region_check_region, NULL, "C", c_major, 7};
    AnalysisResult *no_framework_result;
    assert(key_estimation_analyze(&no_framework_input, NULL, NULL, &no_framework_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: a missing tonic name is rejected. */
    KeyEstimationInput no_tonic_input = {no_region_check_region, framework, NULL, c_major, 7};
    AnalysisResult *no_tonic_result;
    assert(key_estimation_analyze(&no_tonic_input, NULL, NULL, &no_tonic_result) == ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: an empty pitch-class set is rejected. */
    KeyEstimationInput no_pitch_classes_input = {no_region_check_region, framework, "C", NULL, 0};
    AnalysisResult *no_pitch_classes_result;
    assert(key_estimation_analyze(&no_pitch_classes_input, NULL, NULL, &no_pitch_classes_result) ==
           ANALYZER_ERR_NULL_INPUT);

    structure_destroy(no_region_check_region);

    /* Identification: a real major key. */
    Structure *major_region = make_region("structure-c-major", "note-c-major-1");
    KeyEstimationInput major_input = {major_region, framework, "C", c_major, 7};
    AnalysisResult *major_result;
    assert(KEY_ESTIMATION_ANALYZER.analyze(&major_input, NULL, NULL, &major_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(major_result) == 1);
    assert(analysis_result_evidence_count(major_result) == 1);

    const Interpretation *major_interpretation = analysis_result_interpretation_at(major_result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(major_interpretation)), "structure-c-major") == 0);
    assert(interpretation_framework_reference(major_interpretation) != NULL);

    size_t major_claim_size;
    const char *major_claim_content =
        (const char *)value_content(interpretation_claim(major_interpretation), &major_claim_size);
    assert(strcmp(major_claim_content, "C major") == 0);

    analysis_result_destroy(major_result);
    structure_destroy(major_region);

    /* Identification: a real minor key. */
    int a_minor[] = {0, 2, 3, 5, 7, 8, 10};
    Structure *minor_region = make_region("structure-a-minor", "note-a-minor-1");
    KeyEstimationInput minor_input = {minor_region, framework, "A", a_minor, 7};
    AnalysisResult *minor_result;
    assert(KEY_ESTIMATION_ANALYZER.analyze(&minor_input, NULL, NULL, &minor_result) == ANALYZER_OK);

    const Interpretation *minor_interpretation = analysis_result_interpretation_at(minor_result, 0);
    size_t minor_claim_size;
    const char *minor_claim_content =
        (const char *)value_content(interpretation_claim(minor_interpretation), &minor_claim_size);
    assert(strcmp(minor_claim_content, "A minor") == 0);

    analysis_result_destroy(minor_result);
    structure_destroy(minor_region);

    /* No match: neither major nor natural minor -- reported as a failed
       analysis, never a forced guess. */
    int chromatic_cluster[] = {0, 1, 2, 3, 4, 5, 6};
    Structure *no_match_region = make_region("structure-no-match", "note-no-match-1");
    KeyEstimationInput no_match_input = {no_match_region, framework, "C", chromatic_cluster, 7};
    AnalysisResult *no_match_result;
    assert(key_estimation_analyze(&no_match_input, NULL, NULL, &no_match_result) == ANALYZER_ERR_ANALYSIS_FAILED);
    structure_destroy(no_match_region);

    framework_destroy(framework);

    printf("test_key_estimation: ok\n");
    return 0;
}
