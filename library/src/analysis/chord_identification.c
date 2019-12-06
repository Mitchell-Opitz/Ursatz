#include "analysis/chord_identification.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "triad_identification.h"

AnalyzerResult chord_identification_analyze(const void *input, const Context *context, const Value *config,
                                             AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const ChordIdentificationInput *analysis_input = (const ChordIdentificationInput *)input;
    if (analysis_input->sonority == NULL || analysis_input->framework == NULL ||
        analysis_input->scale_degrees == NULL || analysis_input->scale_degree_count == 0) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    const char *anchor_event_id_value = entity_id_value(sonority_event_id_at(analysis_input->sonority, 0));
    const char *framework_name_value = framework_name(analysis_input->framework);

    EntityID *sonority_target_id;
    if (entity_id_create(anchor_event_id_value, &sonority_target_id) != ENTITY_ID_OK) {
        return ANALYZER_ERR_ALLOC;
    }

    Chord *chord;
    if (!common_practice_identify_triad_chord(analysis_input->scale_degrees, analysis_input->scale_degree_count,
                                               sonority_target_id, &chord)) {
        entity_id_destroy(sonority_target_id);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    Interpretation *interpretation = chord_take_interpretation(chord);

    char id_buffer[256];
    snprintf(id_buffer, sizeof(id_buffer), "evidence-chord-identification-%s-%s", anchor_event_id_value,
             framework_name_value);
    EntityID *evidence_id;
    if (entity_id_create(id_buffer, &evidence_id) != ENTITY_ID_OK) {
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ALLOC;
    }
    Evidence *evidence;
    if (evidence_create(evidence_id, 1, 1, 1, 1, 1, 1, false, "diatonic-triad-vocabulary", framework_name_value,
                         &evidence) != EVIDENCE_OK) {
        entity_id_destroy(evidence_id);
        interpretation_destroy(interpretation);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    Interpretation *interpretations[1] = {interpretation};
    Evidence *evidences[1] = {evidence};

    EntityID *result_id;
    snprintf(id_buffer, sizeof(id_buffer), "analysis-result-chord-identification-%s-%s", anchor_event_id_value,
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

const Analyzer CHORD_IDENTIFICATION_ANALYZER = {"ChordIdentification", chord_identification_analyze};
