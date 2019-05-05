#include "analysis/analyzer.h"

#include <stdlib.h>

struct AnalysisResult {
    EntityID *id;
    Interpretation **interpretations;
    size_t interpretation_count;
    Evidence **evidences;
    size_t evidence_count;
    ProvenanceReference *provenance;
};

static void destroy_interpretations(Interpretation **interpretations, size_t count) {
    if (interpretations == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(interpretations[i]);
    }
    free(interpretations);
}

static void destroy_evidences(Evidence **evidences, size_t count) {
    if (evidences == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        evidence_destroy(evidences[i]);
    }
    free(evidences);
}

AnalysisResultResult analysis_result_create(EntityID *id, Interpretation **interpretations,
                                             size_t interpretation_count, Evidence **evidences, size_t evidence_count,
                                             ProvenanceReference *provenance, AnalysisResult **out) {
    if (id == NULL) {
        return ANALYSIS_RESULT_ERR_NULL_ID;
    }
    if (interpretation_count > 0) {
        if (interpretations == NULL) {
            return ANALYSIS_RESULT_ERR_NULL_INTERPRETATION;
        }
        for (size_t i = 0; i < interpretation_count; i++) {
            if (interpretations[i] == NULL) {
                return ANALYSIS_RESULT_ERR_NULL_INTERPRETATION;
            }
        }
    }
    if (evidence_count > 0) {
        if (evidences == NULL) {
            return ANALYSIS_RESULT_ERR_NULL_EVIDENCE;
        }
        for (size_t i = 0; i < evidence_count; i++) {
            if (evidences[i] == NULL) {
                return ANALYSIS_RESULT_ERR_NULL_EVIDENCE;
            }
        }
    }
    if (provenance == NULL) {
        return ANALYSIS_RESULT_ERR_NULL_PROVENANCE;
    }

    AnalysisResult *result = (AnalysisResult *)malloc(sizeof(AnalysisResult));
    if (result == NULL) {
        return ANALYSIS_RESULT_ERR_ALLOC;
    }

    Interpretation **copied_interpretations = NULL;
    if (interpretation_count > 0) {
        copied_interpretations = (Interpretation **)malloc(sizeof(Interpretation *) * interpretation_count);
        if (copied_interpretations == NULL) {
            free(result);
            return ANALYSIS_RESULT_ERR_ALLOC;
        }
        for (size_t i = 0; i < interpretation_count; i++) {
            copied_interpretations[i] = interpretations[i];
        }
    }

    Evidence **copied_evidences = NULL;
    if (evidence_count > 0) {
        copied_evidences = (Evidence **)malloc(sizeof(Evidence *) * evidence_count);
        if (copied_evidences == NULL) {
            free(copied_interpretations);
            free(result);
            return ANALYSIS_RESULT_ERR_ALLOC;
        }
        for (size_t i = 0; i < evidence_count; i++) {
            copied_evidences[i] = evidences[i];
        }
    }

    result->id = id;
    result->interpretations = copied_interpretations;
    result->interpretation_count = interpretation_count;
    result->evidences = copied_evidences;
    result->evidence_count = evidence_count;
    result->provenance = provenance;

    *out = result;
    return ANALYSIS_RESULT_OK;
}

void analysis_result_destroy(AnalysisResult *result) {
    if (result == NULL) {
        return;
    }
    entity_id_destroy(result->id);
    destroy_interpretations(result->interpretations, result->interpretation_count);
    destroy_evidences(result->evidences, result->evidence_count);
    provenance_reference_destroy(result->provenance);
    free(result);
}

const EntityID *analysis_result_id(const AnalysisResult *result) {
    return result->id;
}

size_t analysis_result_interpretation_count(const AnalysisResult *result) {
    return result->interpretation_count;
}

const Interpretation *analysis_result_interpretation_at(const AnalysisResult *result, size_t index) {
    return result->interpretations[index];
}

size_t analysis_result_evidence_count(const AnalysisResult *result) {
    return result->evidence_count;
}

const Evidence *analysis_result_evidence_at(const AnalysisResult *result, size_t index) {
    return result->evidences[index];
}

const ProvenanceReference *analysis_result_provenance(const AnalysisResult *result) {
    return result->provenance;
}