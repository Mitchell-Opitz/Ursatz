#include "analysis/chord_identification.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"

static size_t find_top_candidate(const UncertaintyCandidate *candidates, size_t candidate_count) {
    size_t best = 0;
    for (size_t i = 1; i < candidate_count; i++) {
        /* candidates[i].weight > candidates[best].weight, cross-multiplied to avoid float. */
        unsigned long long lhs = (unsigned long long)candidates[i].weight_numerator *
                                  (unsigned long long)candidates[best].weight_denominator;
        unsigned long long rhs = (unsigned long long)candidates[best].weight_numerator *
                                  (unsigned long long)candidates[i].weight_denominator;
        if (lhs > rhs) {
            best = i;
        }
    }
    return best;
}

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
        analysis_input->candidates == NULL || analysis_input->candidate_count == 0) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    for (size_t i = 0; i < analysis_input->candidate_count; i++) {
        if (analysis_input->candidates[i].label == NULL || analysis_input->candidates[i].weight_denominator == 0) {
            return ANALYZER_ERR_NULL_INPUT;
        }
    }

    const char *anchor_event_id_value = entity_id_value(sonority_event_id_at(analysis_input->sonority, 0));
    const char *framework_name_value = framework_name(analysis_input->framework);
    size_t top = find_top_candidate(analysis_input->candidates, analysis_input->candidate_count);
    const UncertaintyCandidate *top_candidate = &analysis_input->candidates[top];

    char id_buffer[256];

    EntityID *evidence_id;
    snprintf(id_buffer, sizeof(id_buffer), "evidence-chord-identification-%s-%s", anchor_event_id_value,
             framework_name_value);
    if (entity_id_create(id_buffer, &evidence_id) != ENTITY_ID_OK) {
        return ANALYZER_ERR_ALLOC;
    }
    Evidence *evidence;
    if (evidence_create(evidence_id, top_candidate->weight_numerator, top_candidate->weight_denominator, 1, 1, 1, 1,
                         false, "framework-relative chord matching", framework_name_value, &evidence) !=
        EVIDENCE_OK) {
        entity_id_destroy(evidence_id);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    EntityID *interpretation_id;
    snprintf(id_buffer, sizeof(id_buffer), "interpretation-chord-identification-%s-%s", anchor_event_id_value,
             framework_name_value);
    if (entity_id_create(id_buffer, &interpretation_id) != ENTITY_ID_OK) {
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }
    EntityID *target_id;
    if (entity_id_create(anchor_event_id_value, &target_id) != ENTITY_ID_OK) {
        entity_id_destroy(interpretation_id);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }
    EntityID *evidence_ref_id;
    snprintf(id_buffer, sizeof(id_buffer), "evidence-chord-identification-%s-%s", anchor_event_id_value,
             framework_name_value);
    if (entity_id_create(id_buffer, &evidence_ref_id) != ENTITY_ID_OK) {
        entity_id_destroy(target_id);
        entity_id_destroy(interpretation_id);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }

    size_t label_len = strlen(top_candidate->label);
    Value *claim;
    if (value_create("chord-identification-top-candidate", top_candidate->label, label_len + 1, &claim) !=
        VALUE_OK) {
        entity_id_destroy(evidence_ref_id);
        entity_id_destroy(target_id);
        entity_id_destroy(interpretation_id);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }

    Uncertainty *uncertainty;
    if (uncertainty_create_candidate_set(analysis_input->candidates, analysis_input->candidate_count,
                                          &uncertainty) != UNCERTAINTY_OK) {
        value_destroy(claim);
        entity_id_destroy(evidence_ref_id);
        entity_id_destroy(target_id);
        entity_id_destroy(interpretation_id);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        uncertainty_destroy(uncertainty);
        value_destroy(claim);
        entity_id_destroy(evidence_ref_id);
        entity_id_destroy(target_id);
        entity_id_destroy(interpretation_id);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }

    Interpretation *interpretation;
    if (interpretation_create(interpretation_id, target_id, claim, NULL, evidence_ref_id, NULL, uncertainty,
                               "ChordIdentification", "framework-relative chord matching",
                               INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
        INTERPRETATION_OK) {
        evidence_destroy(evidence);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    Interpretation *interpretations[1] = {interpretation};
    Evidence *evidences[1] = {evidence};

    EntityID *result_id;
    snprintf(id_buffer, sizeof(id_buffer), "analysis-result-chord-identification-%s-%s", anchor_event_id_value,
             framework_name_value);
    if (entity_id_create(id_buffer, &result_id) != ENTITY_ID_OK) {
        interpretation_destroy(interpretation);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        interpretation_destroy(interpretation);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, 1, evidences, 1, result_provenance, &result) !=
        ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        interpretation_destroy(interpretation);
        evidence_destroy(evidence);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    *out = result;
    return ANALYZER_OK;
}

const Analyzer CHORD_IDENTIFICATION_ANALYZER = {"ChordIdentification", chord_identification_analyze};