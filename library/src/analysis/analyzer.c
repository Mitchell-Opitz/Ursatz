#include "analysis/analyzer.h"

#include <stdlib.h>

struct AnalysisResult {
    EntityID *id;
    EntityID **interpretation_ids;
    size_t interpretation_id_count;
    ProvenanceReference *provenance;
};

static void destroy_interpretation_ids(EntityID **interpretation_ids, size_t count) {
    if (interpretation_ids == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        entity_id_destroy(interpretation_ids[i]);
    }
    free(interpretation_ids);
}

AnalysisResultResult analysis_result_create(EntityID *id, EntityID **interpretation_ids,
                                             size_t interpretation_id_count, ProvenanceReference *provenance,
                                             AnalysisResult **out) {
    if (id == NULL) {
        return ANALYSIS_RESULT_ERR_NULL_ID;
    }
    if (interpretation_id_count > 0) {
        if (interpretation_ids == NULL) {
            return ANALYSIS_RESULT_ERR_NULL_INTERPRETATION_ID;
        }
        for (size_t i = 0; i < interpretation_id_count; i++) {
            if (interpretation_ids[i] == NULL) {
                return ANALYSIS_RESULT_ERR_NULL_INTERPRETATION_ID;
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

    EntityID **copied_ids = NULL;
    if (interpretation_id_count > 0) {
        copied_ids = (EntityID **)malloc(sizeof(EntityID *) * interpretation_id_count);
        if (copied_ids == NULL) {
            free(result);
            return ANALYSIS_RESULT_ERR_ALLOC;
        }
        for (size_t i = 0; i < interpretation_id_count; i++) {
            copied_ids[i] = interpretation_ids[i];
        }
    }

    result->id = id;
    result->interpretation_ids = copied_ids;
    result->interpretation_id_count = interpretation_id_count;
    result->provenance = provenance;

    *out = result;
    return ANALYSIS_RESULT_OK;
}

void analysis_result_destroy(AnalysisResult *result) {
    if (result == NULL) {
        return;
    }
    entity_id_destroy(result->id);
    destroy_interpretation_ids(result->interpretation_ids, result->interpretation_id_count);
    provenance_reference_destroy(result->provenance);
    free(result);
}

const EntityID *analysis_result_id(const AnalysisResult *result) {
    return result->id;
}

size_t analysis_result_interpretation_id_count(const AnalysisResult *result) {
    return result->interpretation_id_count;
}

const EntityID *analysis_result_interpretation_id_at(const AnalysisResult *result, size_t index) {
    return result->interpretation_ids[index];
}

const ProvenanceReference *analysis_result_provenance(const AnalysisResult *result) {
    return result->provenance;
}