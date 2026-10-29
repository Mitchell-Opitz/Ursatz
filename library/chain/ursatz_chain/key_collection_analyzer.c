#include "ursatz_chain/key_collection_analyzer.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/entity.h"
#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/key.h"
#include "common-practice-minimal/key_identification.h"

AnalyzerResult chain_key_collection_analyze(const void *input, const Context *context, const Value *config,
                                       AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const ChainKeyCollectionInput *analysis_input = (const ChainKeyCollectionInput *)input;
    if (analysis_input->region == NULL || analysis_input->framework == NULL ||
        analysis_input->tonic_name == NULL || analysis_input->tonic_name[0] == '\0' ||
        analysis_input->tonic_relative_pitch_classes == NULL || analysis_input->pitch_class_count == 0) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    const char *region_id_value = entity_id_value(entity_id(structure_entity(analysis_input->region)));
    const char *framework_name_value = framework_name(analysis_input->framework);

    EntityID *context_target_id;
    if (entity_id_create(region_id_value, &context_target_id) != ENTITY_ID_OK) {
        return ANALYZER_ERR_ALLOC;
    }

    Key *key;
    if (!common_practice_identify_key(analysis_input->tonic_relative_pitch_classes, analysis_input->pitch_class_count,
                                       analysis_input->tonic_name, context_target_id, &key)) {
        entity_id_destroy(context_target_id);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    Interpretation *interpretation = key_take_interpretation(key);

    char id_buffer[256];
    snprintf(id_buffer, sizeof(id_buffer), "evidence-key-estimation-%s-%s", region_id_value, framework_name_value);
    EntityID *evidence_id;
    if (entity_id_create(id_buffer, &evidence_id) != ENTITY_ID_OK) {
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ALLOC;
    }
    Evidence *evidence;
    if (evidence_create(evidence_id, 1, 1, 1, 1, 1, 1, false, "major-minor-key-vocabulary", framework_name_value,
                         &evidence) != EVIDENCE_OK) {
        entity_id_destroy(evidence_id);
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    Interpretation *interpretations[1] = {interpretation};
    Evidence *evidences[1] = {evidence};

    EntityID *result_id;
    snprintf(id_buffer, sizeof(id_buffer), "analysis-result-key-estimation-%s-%s", region_id_value,
             framework_name_value);
    if (entity_id_create(id_buffer, &result_id) != ENTITY_ID_OK) {
        evidence_destroy(evidence);
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        evidence_destroy(evidence);
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, 1, evidences, 1, result_provenance, &result) !=
        ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        evidence_destroy(evidence);
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    *out = result;
    return ANALYZER_OK;
}

const Analyzer CHAIN_KEY_COLLECTION_ANALYZER = {"KeyEstimation", chain_key_collection_analyze};
