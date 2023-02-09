#include "analysis/period_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#define PERIOD_DETECTION_ID_BUFFER_SIZE 288

static bool validate_entries(const PeriodDetectionEntry *entries, size_t entry_count) {
    if (entry_count > 0 && entries == NULL) {
        return false;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].phrase_occurrence == NULL) {
            return false;
        }
    }
    return true;
}

/* A cadence with rank <= 0 is not rankable (unknown, invalid, or one that does not close) and
   never part of a period -- not ranked, never guessed at. */
static bool is_period_pair(const PeriodDetectionEntry *antecedent, const PeriodDetectionEntry *consequent,
                           PeriodDetectionRankFn rank) {
    int antecedent_rank = rank(antecedent->cadence_type);
    int consequent_rank = rank(consequent->cadence_type);
    if (antecedent_rank <= 0 || consequent_rank <= 0) {
        return false;
    }
    return antecedent_rank < consequent_rank;
}

static bool make_id(const char *prefix, const char *antecedent_value, const char *consequent_value, EntityID **out) {
    char buffer[PERIOD_DETECTION_ID_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s-%s", prefix, antecedent_value, consequent_value);
    return entity_id_create(buffer, out) == ENTITY_ID_OK;
}

static void destroy_partial_interpretations(Interpretation **interpretations, size_t count) {
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(interpretations[i]);
    }
}

AnalyzerResult period_detection_analyze(const void *input, const Context *context, const Value *config,
                                         AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const PeriodDetectionInput *analysis_input = (const PeriodDetectionInput *)input;
    if (analysis_input->rank == NULL ||
        !validate_entries(analysis_input->entries, analysis_input->entry_count)) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    size_t entry_count = analysis_input->entry_count;
    size_t pair_count = entry_count < 2 ? 0 : entry_count - 1;

    Interpretation **interpretations = NULL;
    if (pair_count > 0) {
        interpretations = (Interpretation **)calloc(pair_count, sizeof(Interpretation *));
        if (interpretations == NULL) {
            return ANALYZER_ERR_ALLOC;
        }
    }

    size_t out_index = 0;
    for (size_t i = 0; i + 1 < entry_count; i++) {
        const PeriodDetectionEntry *antecedent = &analysis_input->entries[i];
        const PeriodDetectionEntry *consequent = &analysis_input->entries[i + 1];
        if (!is_period_pair(antecedent, consequent, analysis_input->rank)) {
            continue;
        }

        const char *antecedent_value = entity_id_value(phrase_occurrence_id(antecedent->phrase_occurrence));
        const char *consequent_value = entity_id_value(phrase_occurrence_id(consequent->phrase_occurrence));

        EntityID *relationship_id;
        if (!make_id("relationship-period", antecedent_value, consequent_value, &relationship_id)) {
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *interpretation_id;
        if (!make_id("interpretation-period", antecedent_value, consequent_value, &interpretation_id)) {
            entity_id_destroy(relationship_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ALLOC;
        }

        PeriodDetectionClaim claim_content;
        memset(&claim_content, 0, sizeof(claim_content));
        snprintf(claim_content.antecedent_phrase_occurrence_id, sizeof(claim_content.antecedent_phrase_occurrence_id),
                  "%s", antecedent_value);
        snprintf(claim_content.consequent_phrase_occurrence_id, sizeof(claim_content.consequent_phrase_occurrence_id),
                  "%s", consequent_value);
        claim_content.antecedent_cadence_type = antecedent->cadence_type;
        claim_content.consequent_cadence_type = consequent->cadence_type;

        Value *claim;
        if (value_create("period-detection-claim", &claim_content, sizeof(claim_content), &claim) != VALUE_OK) {
            entity_id_destroy(interpretation_id);
            entity_id_destroy(relationship_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ALLOC;
        }

        Uncertainty *uncertainty;
        if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
            value_destroy(claim);
            entity_id_destroy(interpretation_id);
            entity_id_destroy(relationship_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }

        ProvenanceReference *provenance;
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            uncertainty_destroy(uncertainty);
            value_destroy(claim);
            entity_id_destroy(interpretation_id);
            entity_id_destroy(relationship_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ALLOC;
        }

        Interpretation *interpretation;
        if (interpretation_create(interpretation_id, relationship_id, claim, NULL, NULL, NULL, uncertainty,
                                   "PeriodDetection", "cadence-strength antecedent/consequent pairing",
                                   INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
            INTERPRETATION_OK) {
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        interpretations[out_index] = interpretation;
        out_index++;
    }

    EntityID *result_id;
    if (entity_id_create("analysis-result-period-detection", &result_id) != ENTITY_ID_OK) {
        destroy_partial_interpretations(interpretations, out_index);
        free(interpretations);
        return ANALYZER_ERR_ALLOC;
    }
    ProvenanceReference *result_provenance;
    if (provenance_reference_create_absent(&result_provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(result_id);
        destroy_partial_interpretations(interpretations, out_index);
        free(interpretations);
        return ANALYZER_ERR_ALLOC;
    }

    AnalysisResult *result;
    if (analysis_result_create(result_id, interpretations, out_index, NULL, 0, result_provenance, &result) !=
        ANALYSIS_RESULT_OK) {
        entity_id_destroy(result_id);
        provenance_reference_destroy(result_provenance);
        destroy_partial_interpretations(interpretations, out_index);
        free(interpretations);
        return ANALYZER_ERR_ANALYSIS_FAILED;
    }

    free(interpretations);
    *out = result;
    return ANALYZER_OK;
}

const Analyzer PERIOD_DETECTION_ANALYZER = {"PeriodDetection", period_detection_analyze};

static void destroy_partial_relationships(Relationship **relationships, size_t count) {
    for (size_t i = 0; i < count; i++) {
        relationship_destroy(relationships[i]);
    }
}

PeriodDetectionRelationshipResult period_detection_build_relationships(const PeriodDetectionEntry *entries,
                                                                         size_t entry_count,
                                                                         PeriodDetectionRankFn rank,
                                                                         Relationship ***out_relationships,
                                                                         size_t *out_relationship_count) {
    if (rank == NULL) {
        return PERIOD_DETECTION_REL_ERR_NULL_RANK;
    }
    if (entry_count > 0 && entries == NULL) {
        return PERIOD_DETECTION_REL_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].phrase_occurrence == NULL) {
            return PERIOD_DETECTION_REL_ERR_NULL_ENTRY;
        }
    }

    size_t pair_count = entry_count < 2 ? 0 : entry_count - 1;
    if (pair_count == 0) {
        *out_relationships = NULL;
        *out_relationship_count = 0;
        return PERIOD_DETECTION_REL_OK;
    }

    Relationship **relationships = (Relationship **)calloc(pair_count, sizeof(Relationship *));
    if (relationships == NULL) {
        return PERIOD_DETECTION_REL_ERR_ALLOC;
    }

    size_t out_index = 0;
    for (size_t i = 0; i + 1 < entry_count; i++) {
        const PeriodDetectionEntry *antecedent = &entries[i];
        const PeriodDetectionEntry *consequent = &entries[i + 1];
        if (!is_period_pair(antecedent, consequent, rank)) {
            continue;
        }

        const char *antecedent_value = entity_id_value(phrase_occurrence_id(antecedent->phrase_occurrence));
        const char *consequent_value = entity_id_value(phrase_occurrence_id(consequent->phrase_occurrence));

        EntityID *relationship_id;
        if (!make_id("relationship-period", antecedent_value, consequent_value, &relationship_id)) {
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return PERIOD_DETECTION_REL_ERR_ALLOC;
        }
        EntityID *source_id;
        if (entity_id_create(antecedent_value, &source_id) != ENTITY_ID_OK) {
            entity_id_destroy(relationship_id);
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return PERIOD_DETECTION_REL_ERR_ALLOC;
        }
        EntityID *target_id;
        if (entity_id_create(consequent_value, &target_id) != ENTITY_ID_OK) {
            entity_id_destroy(source_id);
            entity_id_destroy(relationship_id);
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return PERIOD_DETECTION_REL_ERR_ALLOC;
        }
        ProvenanceReference *provenance;
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            entity_id_destroy(target_id);
            entity_id_destroy(source_id);
            entity_id_destroy(relationship_id);
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return PERIOD_DETECTION_REL_ERR_ALLOC;
        }

        Relationship *relationship;
        if (relationship_create(relationship_id, source_id, "precedes", target_id, NULL, NULL, NULL, NULL, NULL,
                                 provenance, &relationship) != RELATIONSHIP_OK) {
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return PERIOD_DETECTION_REL_ERR_ALLOC;
        }
        relationships[out_index] = relationship;
        out_index++;
    }

    *out_relationships = relationships;
    *out_relationship_count = out_index;
    return PERIOD_DETECTION_REL_OK;
}

void period_detection_destroy_relationships(Relationship **relationships, size_t relationship_count) {
    if (relationships == NULL) {
        return;
    }
    for (size_t i = 0; i < relationship_count; i++) {
        relationship_destroy(relationships[i]);
    }
    free(relationships);
}
