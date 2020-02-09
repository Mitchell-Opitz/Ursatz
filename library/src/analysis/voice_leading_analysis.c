#include "analysis/voice_leading_analysis.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#define VOICE_LEADING_ANALYSIS_ID_BUFFER_SIZE 256

static bool make_id(const char *prefix, const char *a_value, const char *b_value, EntityID **out) {
    char buffer[VOICE_LEADING_ANALYSIS_ID_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s-%s", prefix, a_value, b_value);
    return entity_id_create(buffer, out) == ENTITY_ID_OK;
}

static void destroy_partial(Interpretation **interpretations, size_t interpretation_count, Evidence **evidences,
                             size_t evidence_count) {
    for (size_t i = 0; i < interpretation_count; i++) {
        interpretation_destroy(interpretations[i]);
    }
    for (size_t i = 0; i < evidence_count; i++) {
        evidence_destroy(evidences[i]);
    }
}

AnalyzerResult voice_leading_analysis_analyze(const void *input, const Context *context, const Value *config,
                                               AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const VoiceLeadingAnalysisInput *analysis_input = (const VoiceLeadingAnalysisInput *)input;
    size_t pair_count = analysis_input->pair_count;

    if (pair_count > 0 && analysis_input->pairs == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    for (size_t i = 0; i < pair_count; i++) {
        if (analysis_input->pairs[i].voice_a_to == NULL || analysis_input->pairs[i].voice_b_to == NULL) {
            return ANALYZER_ERR_NULL_INPUT;
        }
    }

    Interpretation **interpretations = NULL;
    Evidence **evidences = NULL;
    if (pair_count > 0) {
        interpretations = (Interpretation **)calloc(pair_count, sizeof(Interpretation *));
        evidences = (Evidence **)calloc(pair_count, sizeof(Evidence *));
        if (interpretations == NULL || evidences == NULL) {
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
    }

    for (size_t i = 0; i < pair_count; i++) {
        const VoiceLeadingAnalysisPair *pair = &analysis_input->pairs[i];
        const char *a_value = entity_id_value(note_event_id(pair->voice_a_to));
        const char *b_value = entity_id_value(note_event_id(pair->voice_b_to));

        EntityID *evidence_id;
        if (!make_id("evidence-voice-leading-analysis", a_value, b_value, &evidence_id)) {
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        Evidence *evidence;
        if (evidence_create(evidence_id, 1, 1, 1, 1, 1, 1, true, "voice-pair motion classification",
                             "VoiceLeadingAnalysis", &evidence) != EVIDENCE_OK) {
            entity_id_destroy(evidence_id);
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        evidences[i] = evidence;

        EntityID *interpretation_id;
        if (!make_id("interpretation-voice-leading-analysis", a_value, b_value, &interpretation_id)) {
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *target_id;
        if (entity_id_create(a_value, &target_id) != ENTITY_ID_OK) {
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *evidence_ref_id;
        if (!make_id("evidence-voice-leading-analysis", a_value, b_value, &evidence_ref_id)) {
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        VoiceLeadingClaim claim_content = {pair->motion, pair->resulting_interval_is_perfect, pair->bass_motion};
        Value *claim;
        if (value_create("voice-leading-analysis-claim", &claim_content, sizeof(claim_content), &claim) !=
            VALUE_OK) {
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        Uncertainty *uncertainty;
        if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
            value_destroy(claim);
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        ProvenanceReference *provenance;
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            uncertainty_destroy(uncertainty);
            value_destroy(claim);
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        Interpretation *interpretation;
        if (interpretation_create(interpretation_id, target_id, claim, NULL, evidence_ref_id, NULL, uncertainty,
                                   "VoiceLeadingAnalysis", "voice-pair motion classification",
                                   INTERPRETATION_STATUS_MACHINE_GENERATED, provenance,
                                   &interpretation) != INTERPRETATION_OK) {
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        interpretations[i] = interpretation;
    }

    EntityID *result_id;
    if (entity_id_create("analysis-result-voice-leading-analysis", &result_id) != ENTITY_ID_OK) {
        destroy_partial(interpretations, pair_count, evidences, pair_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        destroy_partial(interpretations, pair_count, evidences, pair_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, pair_count, evidences, pair_count, result_provenance,
                                &result) != ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        destroy_partial(interpretations, pair_count, evidences, pair_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    free(interpretations);
    free(evidences);
    *out = result;
    return ANALYZER_OK;
}

const Analyzer VOICE_LEADING_ANALYSIS_ANALYZER = {"VoiceLeadingAnalysis", voice_leading_analysis_analyze};