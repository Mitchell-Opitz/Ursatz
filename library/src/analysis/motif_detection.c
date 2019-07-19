#include "analysis/motif_detection.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/occurrence.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "structure/motif_occurrence.h"

#define MOTIF_DETECTION_ID_BUFFER_SIZE 256

static bool copy_id(const EntityID *source, EntityID **out) {
    return entity_id_create(entity_id_value(source), out) == ENTITY_ID_OK;
}

static bool make_id(const char *prefix, const char *motif_value, const char *anchor_value, EntityID **out) {
    char buffer[MOTIF_DETECTION_ID_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s-%s", prefix, motif_value, anchor_value);
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

/* Builds a transient MotifOccurrence purely to validate occurrence's reference list, returning a
   fresh copy of its own id (occurrence's identity) on success. The MotifOccurrence itself is
   destroyed before returning -- its data has already been captured by the caller's Interpretation. */
static bool validate_and_capture_occurrence_id(const Motif *motif, const MotifDetectionOccurrence *occurrence,
                                                const char *motif_value, const char *anchor_value, EntityID **out) {
    EntityID *occurrence_own_id;
    if (!make_id("occurrence-motif-detection", motif_value, anchor_value, &occurrence_own_id)) {
        return false;
    }
    EntityID *target_id;
    if (!copy_id(motif_id(motif), &target_id)) {
        entity_id_destroy(occurrence_own_id);
        return false;
    }
    Occurrence *base_occurrence;
    if (occurrence_create(occurrence_own_id, target_id, &base_occurrence) != OCCURRENCE_OK) {
        entity_id_destroy(target_id);
        entity_id_destroy(occurrence_own_id);
        return false;
    }

    EntityID **region_event_ids = (EntityID **)malloc(sizeof(EntityID *) * occurrence->matched_event_id_count);
    if (region_event_ids == NULL) {
        occurrence_destroy(base_occurrence);
        return false;
    }
    for (size_t i = 0; i < occurrence->matched_event_id_count; i++) {
        if (!copy_id(occurrence->matched_event_ids[i], &region_event_ids[i])) {
            for (size_t j = 0; j < i; j++) {
                entity_id_destroy(region_event_ids[j]);
            }
            free(region_event_ids);
            occurrence_destroy(base_occurrence);
            return false;
        }
    }

    MotifOccurrence *motif_occurrence;
    if (motif_occurrence_create(base_occurrence, region_event_ids, occurrence->matched_event_id_count,
                                 &motif_occurrence) != MOTIF_OCCURRENCE_OK) {
        for (size_t i = 0; i < occurrence->matched_event_id_count; i++) {
            entity_id_destroy(region_event_ids[i]);
        }
        free(region_event_ids);
        occurrence_destroy(base_occurrence);
        return false;
    }
    /* motif_occurrence_create copies region_event_ids' entries into its own internally-owned
       array rather than taking the array pointer itself -- the outer array is still ours to free. */
    free(region_event_ids);

    bool copied = copy_id(motif_occurrence_id(motif_occurrence), out);
    motif_occurrence_destroy(motif_occurrence);
    return copied;
}

AnalyzerResult motif_detection_analyze(const void *input, const Context *context, const Value *config,
                                        AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const MotifDetectionInput *analysis_input = (const MotifDetectionInput *)input;
    if (analysis_input->motif == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    size_t occurrence_count = analysis_input->occurrence_count;
    if (occurrence_count > 0 && analysis_input->occurrences == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    for (size_t i = 0; i < occurrence_count; i++) {
        const MotifDetectionOccurrence *occurrence = &analysis_input->occurrences[i];
        if (occurrence->matched_event_id_count == 0 || occurrence->matched_event_ids == NULL ||
            occurrence->confidence_denominator == 0 ||
            occurrence->confidence_numerator > occurrence->confidence_denominator) {
            return ANALYZER_ERR_NULL_INPUT;
        }
        for (size_t j = 0; j < occurrence->matched_event_id_count; j++) {
            if (occurrence->matched_event_ids[j] == NULL) {
                return ANALYZER_ERR_NULL_INPUT;
            }
        }
    }

    const char *motif_value = entity_id_value(motif_id(analysis_input->motif));

    Interpretation **interpretations = NULL;
    Evidence **evidences = NULL;
    if (occurrence_count > 0) {
        interpretations = (Interpretation **)calloc(occurrence_count, sizeof(Interpretation *));
        evidences = (Evidence **)calloc(occurrence_count, sizeof(Evidence *));
        if (interpretations == NULL || evidences == NULL) {
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
    }

    for (size_t i = 0; i < occurrence_count; i++) {
        const MotifDetectionOccurrence *occurrence = &analysis_input->occurrences[i];
        const char *anchor_value = entity_id_value(occurrence->matched_event_ids[0]);

        EntityID *target_id;
        if (!validate_and_capture_occurrence_id(analysis_input->motif, occurrence, motif_value, anchor_value,
                                                 &target_id)) {
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }

        EntityID *evidence_id;
        if (!make_id("evidence-motif-detection", motif_value, anchor_value, &evidence_id)) {
            entity_id_destroy(target_id);
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        Evidence *evidence;
        if (evidence_create(evidence_id, occurrence->confidence_numerator, occurrence->confidence_denominator, 1, 1,
                             1, 1, false, "motif-occurrence pattern matching", motif_value, &evidence) !=
            EVIDENCE_OK) {
            entity_id_destroy(evidence_id);
            entity_id_destroy(target_id);
            destroy_partial(interpretations, i, evidences, i);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        evidences[i] = evidence;

        EntityID *interpretation_id;
        if (!make_id("interpretation-motif-detection", motif_value, anchor_value, &interpretation_id)) {
            entity_id_destroy(target_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *evidence_ref_id;
        if (!make_id("evidence-motif-detection", motif_value, anchor_value, &evidence_ref_id)) {
            entity_id_destroy(interpretation_id);
            entity_id_destroy(target_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        MotifDetectionClaim claim_content = {occurrence->matched_event_id_count};
        Value *claim;
        if (value_create("motif-detection-claim", &claim_content, sizeof(claim_content), &claim) != VALUE_OK) {
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(interpretation_id);
            entity_id_destroy(target_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        Uncertainty *uncertainty;
        if (uncertainty_create_scalar_confidence(occurrence->confidence_numerator, occurrence->confidence_denominator,
                                                  &uncertainty) != UNCERTAINTY_OK) {
            value_destroy(claim);
            entity_id_destroy(evidence_ref_id);
            entity_id_destroy(interpretation_id);
            entity_id_destroy(target_id);
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
            entity_id_destroy(interpretation_id);
            entity_id_destroy(target_id);
            destroy_partial(interpretations, i, evidences, i + 1);
            free(interpretations);
            free(evidences);
            return ANALYZER_ERR_ALLOC;
        }

        Interpretation *interpretation;
        if (interpretation_create(interpretation_id, target_id, claim, NULL, evidence_ref_id, NULL, uncertainty,
                                   "MotifDetection", "motif-occurrence pattern matching",
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
    if (!make_id("analysis-result-motif-detection", motif_value, "all", &result_id)) {
        destroy_partial(interpretations, occurrence_count, evidences, occurrence_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        destroy_partial(interpretations, occurrence_count, evidences, occurrence_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, occurrence_count, evidences, occurrence_count,
                                result_provenance, &result) != ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        destroy_partial(interpretations, occurrence_count, evidences, occurrence_count);
        free(interpretations);
        free(evidences);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    free(interpretations);
    free(evidences);
    *out = result;
    return ANALYZER_OK;
}

const Analyzer MOTIF_DETECTION_ANALYZER = {"MotifDetection", motif_detection_analyze};