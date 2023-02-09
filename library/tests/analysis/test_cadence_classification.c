#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/cadence_classification.h"
#include "semantics/cadence_type.h"
#include "core/entity.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "structure/structure.h"

/*
 * Behavioral-contract test: CadenceClassification conforms to the
 * Analyzer interface -- NULL input and an out-of-range root degree are
 * rejected without touching *out.
 *
 * Analysis-regression tests: fixed root-motion inputs (matching real
 * ChordIdentification scale-degree output) always classify to the same
 * cadence type, at the same confidence tier, and degrade to
 * "unclassified" rather than guessing when the distinguishing data
 * (soprano resolution) isn't supplied -- pinning CadenceClassification's
 * rules against a future refactor.
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

static const char *top_label(const AnalysisResult *result) {
    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    size_t claim_size;
    return (const char *)value_content(interpretation_claim(interpretation), &claim_size);
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(CADENCE_CLASSIFICATION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(CADENCE_CLASSIFICATION_ANALYZER.name, "CadenceClassification") == 0);

    Structure *region = make_region("structure-phrase-end-1", "note-g4");

    /* Behavioral contract: an out-of-range root degree is rejected. */
    CadenceClassificationInput bad_input = {region, NULL, 8, 1, false, false, false, false,
                                             VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *bad_result;
    assert(cadence_classification_analyze(&bad_input, NULL, NULL, &bad_result) == ANALYZER_ERR_NULL_INPUT);

    /* Regression: V7-I with no soprano data classifies as unclassified authentic, not a guessed PAC/IAC. */
    CadenceClassificationInput authentic_unclassified = {region,
                                                           NULL,
                                                           5,
                                                           1,
                                                           true,
                                                           false,
                                                           false,
                                                           false,
                                                           VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *authentic_unclassified_result;
    assert(cadence_classification_analyze(&authentic_unclassified, NULL, NULL, &authentic_unclassified_result) ==
           ANALYZER_OK);
    assert(strcmp(top_label(authentic_unclassified_result), cadence_type_label(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED)) == 0);
    const Interpretation *authentic_unclassified_interpretation =
        analysis_result_interpretation_at(authentic_unclassified_result, 0);
    const Uncertainty *authentic_unclassified_uncertainty =
        interpretation_uncertainty(authentic_unclassified_interpretation);
    assert(uncertainty_candidate_weight_numerator_at(authentic_unclassified_uncertainty, 0) == 7);
    assert(uncertainty_candidate_weight_denominator_at(authentic_unclassified_uncertainty, 0) == 8);
    analysis_result_destroy(authentic_unclassified_result);

    /* Regression: V7-I with soprano resolving to tonic classifies as PAC. */
    CadenceClassificationInput pac_input = {region, NULL, 5, 1, true, true, true, false,
                                             VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *pac_result;
    assert(cadence_classification_analyze(&pac_input, NULL, NULL, &pac_result) == ANALYZER_OK);
    assert(strcmp(top_label(pac_result), cadence_type_label(CADENCE_TYPE_PERFECT_AUTHENTIC)) == 0);
    analysis_result_destroy(pac_result);

    /* Regression: V-I with soprano NOT resolving to tonic classifies as IAC. */
    CadenceClassificationInput iac_input = {region, NULL, 5, 1, false, true, false, false,
                                             VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *iac_result;
    assert(cadence_classification_analyze(&iac_input, NULL, NULL, &iac_result) == ANALYZER_OK);
    assert(strcmp(top_label(iac_result), cadence_type_label(CADENCE_TYPE_IMPERFECT_AUTHENTIC)) == 0);
    analysis_result_destroy(iac_result);

    /* Regression: V-I with a caller-supplied bass leap strengthens (but doesn't require) the authentic reading. */
    CadenceClassificationInput bass_leap_input = {region, NULL, 5, 1, false, false, false, true,
                                                   VOICE_LEADING_BASS_MOTION_LEAP};
    AnalysisResult *bass_leap_result;
    assert(cadence_classification_analyze(&bass_leap_input, NULL, NULL, &bass_leap_result) == ANALYZER_OK);
    const Interpretation *bass_leap_interpretation = analysis_result_interpretation_at(bass_leap_result, 0);
    const Uncertainty *bass_leap_uncertainty = interpretation_uncertainty(bass_leap_interpretation);
    assert(uncertainty_candidate_weight_numerator_at(bass_leap_uncertainty, 0) == 7);
    assert(uncertainty_candidate_weight_denominator_at(bass_leap_uncertainty, 0) == 8);
    analysis_result_destroy(bass_leap_result);

    /* Regression: IV-I classifies as plagal. */
    CadenceClassificationInput plagal_input = {region, NULL, 4, 1, false, false, false, false,
                                                VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *plagal_result;
    assert(cadence_classification_analyze(&plagal_input, NULL, NULL, &plagal_result) == ANALYZER_OK);
    assert(strcmp(top_label(plagal_result), cadence_type_label(CADENCE_TYPE_PLAGAL)) == 0);
    analysis_result_destroy(plagal_result);

    /* Regression: ending on V (e.g. I-V) classifies as half cadence. */
    CadenceClassificationInput hc_input = {region, NULL, 1, 5, false, false, false, false,
                                            VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *hc_result;
    assert(cadence_classification_analyze(&hc_input, NULL, NULL, &hc_result) == ANALYZER_OK);
    assert(strcmp(top_label(hc_result), cadence_type_label(CADENCE_TYPE_HALF)) == 0);
    analysis_result_destroy(hc_result);

    /* Regression: V-vi classifies as deceptive. */
    CadenceClassificationInput deceptive_input = {region, NULL, 5, 6, false, false, false, false,
                                                   VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *deceptive_result;
    assert(cadence_classification_analyze(&deceptive_input, NULL, NULL, &deceptive_result) == ANALYZER_OK);
    assert(strcmp(top_label(deceptive_result), cadence_type_label(CADENCE_TYPE_DECEPTIVE)) == 0);
    analysis_result_destroy(deceptive_result);

    /* Regression: unrecognized motion (e.g. ii-iii) reports a bare "unclassified" with no false confidence. */
    CadenceClassificationInput unclassified_input = {region, NULL, 2, 3, false, false, false, false,
                                                       VOICE_LEADING_BASS_MOTION_SAME};
    AnalysisResult *unclassified_result;
    assert(cadence_classification_analyze(&unclassified_input, NULL, NULL, &unclassified_result) == ANALYZER_OK);
    assert(strcmp(top_label(unclassified_result), cadence_type_label(CADENCE_TYPE_UNKNOWN)) == 0);
    const Interpretation *unclassified_interpretation = analysis_result_interpretation_at(unclassified_result, 0);
    const Uncertainty *unclassified_uncertainty = interpretation_uncertainty(unclassified_interpretation);
    assert(uncertainty_candidate_count(unclassified_uncertainty) == 1);
    analysis_result_destroy(unclassified_result);

    structure_destroy(region);

    printf("test_cadence_classification: ok\n");
    return 0;
}
