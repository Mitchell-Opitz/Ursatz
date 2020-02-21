#include "analysis/phrase_boundary_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#define PHRASE_BOUNDARY_DETECTION_ID_BUFFER_SIZE 256

static bool validate_input(const PhraseBoundaryDetectionInput *input) {
    if (input->point_count > 0 && input->points == NULL) {
        return false;
    }
    for (size_t i = 0; i < input->point_count; i++) {
        const PhraseBoundaryDetectionPoint *point = &input->points[i];
        if (point->event == NULL) {
            return false;
        }
        if (point->has_cadence) {
            if (point->cadence_type == NULL ||
                strlen(point->cadence_type) + 1 > PHRASE_BOUNDARY_DETECTION_CADENCE_TYPE_BUFFER_SIZE) {
                return false;
            }
        }
    }
    return true;
}

/* Shared is_boundary/signal computation: the single source of truth both AnalyzerFn
   entry points below rely on for an identical boundary decision per point. */
static void compute_boundaries(const PhraseBoundaryDetectionPoint *points, size_t point_count,
                                size_t max_length_without_cadence, bool *out_is_boundary,
                                PhraseBoundarySignal *out_signal) {
    size_t max_length =
        max_length_without_cadence == 0 ? PHRASE_BOUNDARY_DETECTION_DEFAULT_MAX_LENGTH : max_length_without_cadence;
    size_t since_boundary = 0;

    for (size_t i = 0; i < point_count; i++) {
        const PhraseBoundaryDetectionPoint *point = &points[i];
        bool is_boundary = false;
        PhraseBoundarySignal signal = PHRASE_BOUNDARY_SIGNAL_CADENCE;

        if (point->has_cadence) {
            is_boundary = true;
            signal = PHRASE_BOUNDARY_SIGNAL_CADENCE;
        } else if (since_boundary + 1 >= max_length && point->has_discontinuity) {
            is_boundary = true;
            signal = PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM;
        }

        out_is_boundary[i] = is_boundary;
        if (out_signal != NULL) {
            out_signal[i] = signal;
        }

        since_boundary = is_boundary ? 0 : since_boundary + 1;
    }
}

static bool make_id(const char *prefix, const char *event_value, EntityID **out) {
    char buffer[PHRASE_BOUNDARY_DETECTION_ID_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s", prefix, event_value);
    return entity_id_create(buffer, out) == ENTITY_ID_OK;
}

AnalyzerResult phrase_boundary_detection_analyze(const void *input, const Context *context, const Value *config,
                                                  AnalysisResult **out) {
    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const PhraseBoundaryDetectionInput *analysis_input = (const PhraseBoundaryDetectionInput *)input;
    if (!validate_input(analysis_input)) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    size_t point_count = analysis_input->point_count;
    if (point_count == 0) {
        PhraseSegmentationInput packaged_input = {NULL, 0};
        return phrase_segmentation_analyze(&packaged_input, context, config, out);
    }

    bool *is_boundary = (bool *)calloc(point_count, sizeof(bool));
    PhraseSegmentationPoint *packaged_points =
        (PhraseSegmentationPoint *)calloc(point_count, sizeof(PhraseSegmentationPoint));
    if (is_boundary == NULL || packaged_points == NULL) {
        free(is_boundary);
        free(packaged_points);
        return ANALYZER_ERR_ALLOC;
    }

    compute_boundaries(analysis_input->points, point_count, analysis_input->max_length_without_cadence, is_boundary,
                        NULL);

    for (size_t i = 0; i < point_count; i++) {
        packaged_points[i].event = analysis_input->points[i].event;
        packaged_points[i].is_boundary = is_boundary[i];
        packaged_points[i].confidence_numerator = 1;
        packaged_points[i].confidence_denominator = 1;
    }
    free(is_boundary);

    PhraseSegmentationInput packaged_input = {packaged_points, point_count};
    AnalyzerResult result = phrase_segmentation_analyze(&packaged_input, context, config, out);
    free(packaged_points);
    return result;
}

const Analyzer PHRASE_BOUNDARY_DETECTION_ANALYZER = {"PhraseBoundaryDetection", phrase_boundary_detection_analyze};

static void destroy_partial(Interpretation **interpretations, size_t interpretation_count, Evidence **evidences,
                             size_t evidence_count) {
    for (size_t i = 0; i < interpretation_count; i++) {
        interpretation_destroy(interpretations[i]);
    }
    for (size_t i = 0; i < evidence_count; i++) {
        evidence_destroy(evidences[i]);
    }
}

AnalyzerResult phrase_boundary_detection_signal_analyze(const void *input, const Context *context,
                                                         const Value *config, AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const PhraseBoundaryDetectionInput *analysis_input = (const PhraseBoundaryDetectionInput *)input;
    if (!validate_input(analysis_input)) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    size_t point_count = analysis_input->point_count;
    bool *is_boundary = NULL;
    PhraseBoundarySignal *signals = NULL;
    if (point_count > 0) {
        is_boundary = (bool *)calloc(point_count, sizeof(bool));
        signals = (PhraseBoundarySignal *)calloc(point_count, sizeof(PhraseBoundarySignal));
        if (is_boundary == NULL || signals == NULL) {
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }
        compute_boundaries(analysis_input->points, point_count, analysis_input->max_length_without_cadence,
                            is_boundary, signals);
    }

    size_t boundary_count = 0;
    for (size_t i = 0; i < point_count; i++) {
        if (is_boundary[i]) {
            boundary_count++;
        }
    }

    Interpretation **interpretations = NULL;
    Evidence **evidences = NULL;
    if (boundary_count > 0) {
        interpretations = (Interpretation **)calloc(boundary_count, sizeof(Interpretation *));
        evidences = (Evidence **)calloc(boundary_count, sizeof(Evidence *));
        if (interpretations == NULL || evidences == NULL) {
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }
    }

    size_t out_index = 0;
    for (size_t i = 0; i < point_count; i++) {
        if (!is_boundary[i]) {
            continue;
        }
        const PhraseBoundaryDetectionPoint *point = &analysis_input->points[i];
        const char *event_value = entity_id_value(note_event_id(point->event));

        EntityID *evidence_id;
        if (!make_id("evidence-phrase-boundary-signal", event_value, &evidence_id)) {
            destroy_partial(interpretations, out_index, evidences, out_index);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }
        Evidence *evidence;
        const char *method = signals[i] == PHRASE_BOUNDARY_SIGNAL_CADENCE ? "cadence boundary signal"
                                                                            : "contour/rhythm discontinuity signal";
        if (evidence_create(evidence_id, 1, 1, 1, 1, 1, 1, true, method, "PhraseBoundaryDetection", &evidence) !=
            EVIDENCE_OK) {
            entity_id_destroy(evidence_id);
            destroy_partial(interpretations, out_index, evidences, out_index);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        evidences[out_index] = evidence;

        EntityID *interpretation_id;
        if (!make_id("interpretation-phrase-boundary-signal", event_value, &interpretation_id)) {
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *target_id;
        if (entity_id_create(event_value, &target_id) != ENTITY_ID_OK) {
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *evidence_ref_id;
        if (!make_id("evidence-phrase-boundary-signal", event_value, &evidence_ref_id)) {
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }

        PhraseBoundaryDetectionSignalClaim claim_content;
        claim_content.signal = signals[i];
        claim_content.cadence_type[0] = '\0';
        if (signals[i] == PHRASE_BOUNDARY_SIGNAL_CADENCE) {
            snprintf(claim_content.cadence_type, sizeof(claim_content.cadence_type), "%s", point->cadence_type);
        }
        Value *claim;
        if (value_create("phrase-boundary-detection-signal-claim", &claim_content, sizeof(claim_content), &claim) !=
            VALUE_OK) {
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }

        unsigned long confidence_numerator = 1;
        unsigned long confidence_denominator = 1;
        if (signals[i] == PHRASE_BOUNDARY_SIGNAL_CONTOUR_RHYTHM) {
            confidence_numerator = PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_NUM;
            confidence_denominator = PHRASE_BOUNDARY_DETECTION_DISCONTINUITY_CONFIDENCE_DEN;
        }

        Uncertainty *uncertainty;
        if (uncertainty_create_scalar_confidence(confidence_numerator, confidence_denominator, &uncertainty) !=
            UNCERTAINTY_OK) {
            value_destroy(claim);
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }

        ProvenanceReference *provenance;
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            uncertainty_destroy(uncertainty);
            value_destroy(claim);
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(target_id);
            entity_id_destroy(interpretation_id);
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ALLOC;
        }

        Interpretation *interpretation;
        if (interpretation_create(interpretation_id, target_id, claim, NULL, evidence_ref_id, NULL, uncertainty,
                                   "PhraseBoundaryDetection", method, INTERPRETATION_STATUS_MACHINE_GENERATED,
                                   provenance, &interpretation) != INTERPRETATION_OK) {
            destroy_partial(interpretations, out_index, evidences, out_index + 1);
            free(interpretations);
            free(evidences);
            free(is_boundary);
            free(signals);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        interpretations[out_index] = interpretation;
        out_index++;
    }

    free(is_boundary);
    free(signals);

    EntityID *result_id;
    if (entity_id_create("analysis-result-phrase-boundary-signal", &result_id) != ENTITY_ID_OK) {
        destroy_partial(interpretations, boundary_count, evidences, boundary_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        destroy_partial(interpretations, boundary_count, evidences, boundary_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, boundary_count, evidences, boundary_count,
                                result_provenance, &result) != ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        destroy_partial(interpretations, boundary_count, evidences, boundary_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    free(interpretations);
    free(evidences);
    *out = result;
    return ANALYZER_OK;
}

const Analyzer PHRASE_BOUNDARY_DETECTION_SIGNAL_ANALYZER = {"PhraseBoundaryDetectionSignal",
                                                              phrase_boundary_detection_signal_analyze};
