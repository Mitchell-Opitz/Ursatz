#include "analysis/phrase_segmentation.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#define PHRASE_SEGMENTATION_ID_BUFFER_SIZE 256

static bool make_id(const char *prefix, const char *event_value, EntityID **out) {
    char buffer[PHRASE_SEGMENTATION_ID_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s", prefix, event_value);
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

AnalyzerResult phrase_segmentation_analyze(const void *input, const Context *context, const Value *config,
                                            AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const PhraseSegmentationInput *analysis_input = (const PhraseSegmentationInput *)input;
    size_t point_count = analysis_input->point_count;

    if (point_count > 0 && analysis_input->points == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    for (size_t i = 0; i < point_count; i++) {
        const PhraseSegmentationPoint *point = &analysis_input->points[i];
        if (point->event == NULL || point->confidence_denominator == 0 ||
            point->confidence_numerator > point->confidence_denominator) {
            return ANALYZER_ERR_NULL_INPUT;
        }
    }

    Interpretation **interpretations = NULL;
    Evidence **evidences = NULL;
    if (point_count > 0) {
        interpretations = (Interpretation **)calloc(point_count, sizeof(Interpretation *));
        evidences = (Evidence **)calloc(point_count, sizeof(Evidence *));
        if (interpretations == NULL || evidences == NULL) {
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
    }

    for (size_t i = 0; i < point_count; i++) {
        const PhraseSegmentationPoint *point = &analysis_input->points[i];
        const char *event_value = entity_id_value(note_event_id(point->event));

        EntityID *evidence_id;
        if (!make_id("evidence-phrase-segmentation", event_value, &evidence_id)) {
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        Evidence *evidence;
        if (evidence_create(evidence_id, point->confidence_numerator, point->confidence_denominator, 1, 1, 1, 1,
                             true, "phrase-boundary classification", "PhraseSegmentation", &evidence) !=
            EVIDENCE_OK) {
            entity_id_destroy(evidence_id);
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        evidences[i] = evidence;

        EntityID *interpretation_id;
        if (!make_id("interpretation-phrase-segmentation", event_value, &interpretation_id)) {
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *target_id;
        if (entity_id_create(event_value, &target_id) != ENTITY_ID_OK) {
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *evidence_ref_id;
        if (!make_id("evidence-phrase-segmentation", event_value, &evidence_ref_id)) {
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        PhraseSegmentationClaim claim_content = {point->is_boundary};
        Value *claim;
        if (value_create("phrase-segmentation-claim", &claim_content, sizeof(claim_content), &claim) != VALUE_OK) {
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        Uncertainty *uncertainty;
        if (uncertainty_create_scalar_confidence(point->confidence_numerator, point->confidence_denominator,
                                                  &uncertainty) != UNCERTAINTY_OK) {
            value_destroy(claim);
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ANALYSIS_FAILED;
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
        if (interpretation_create(interpretation_id, target_id, claim, NULL, evidence_ref_id, uncertainty,
                                   "PhraseSegmentation", "phrase-boundary classification",
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
    if (entity_id_create("analysis-result-phrase-segmentation", &result_id) != ENTITY_ID_OK) {
        destroy_partial(interpretations, point_count, evidences, point_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        destroy_partial(interpretations, point_count, evidences, point_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, point_count, evidences, point_count, result_provenance,
                                &result) != ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        destroy_partial(interpretations, point_count, evidences, point_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    free(interpretations);
    free(evidences);
    *out = result;
    return ANALYZER_OK;
}

const Analyzer PHRASE_SEGMENTATION_ANALYZER = {"PhraseSegmentation", phrase_segmentation_analyze};