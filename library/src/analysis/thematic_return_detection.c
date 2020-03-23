#include "analysis/thematic_return_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"

#define THEMATIC_RETURN_DETECTION_ID_MINT_BUFFER_SIZE 288

/* Reuses MotifWindowMatching's exact comparison rule (contour direction equality AND
   rhythmic duration equality, element for element) -- see this module's header comment
   for why the pairwise bookkeeping is reimplemented rather than calling
   motif_window_matching_find directly. */
static bool windows_match(const MotifWindow *a, const MotifWindow *b) {
    size_t direction_count = contour_direction_count(a->contour);
    if (direction_count != contour_direction_count(b->contour)) {
        return false;
    }
    for (size_t i = 0; i < direction_count; i++) {
        if (contour_direction_at(a->contour, i) != contour_direction_at(b->contour, i)) {
            return false;
        }
    }

    size_t duration_count = rhythmic_pattern_duration_count(a->rhythm);
    if (duration_count != rhythmic_pattern_duration_count(b->rhythm)) {
        return false;
    }
    for (size_t i = 0; i < duration_count; i++) {
        if (!duration_equals(rhythmic_pattern_duration_at(a->rhythm, i), rhythmic_pattern_duration_at(b->rhythm, i))) {
            return false;
        }
    }

    return true;
}

static bool valid_entry_shape(const ThematicReturnDetectionEntry *entry) {
    if (entry->theme_occurrence_id == NULL || entry->window == NULL) {
        return false;
    }
    return entry->window->contour != NULL && entry->window->rhythm != NULL && entry->window->event_ids != NULL &&
           entry->window->event_id_count > 0;
}

static bool validate_entries(const ThematicReturnDetectionEntry *entries, size_t entry_count) {
    if (entry_count > 0 && entries == NULL) {
        return false;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (!valid_entry_shape(&entries[i])) {
            return false;
        }
    }
    return true;
}

static bool make_id(const char *prefix, const char *value_a, const char *value_b, EntityID **out) {
    char buffer[THEMATIC_RETURN_DETECTION_ID_MINT_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s-%s", prefix, value_a, value_b);
    return entity_id_create(buffer, out) == ENTITY_ID_OK;
}

static void destroy_partial_interpretations(Interpretation **interpretations, size_t count) {
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(interpretations[i]);
    }
}

static void destroy_partial_relationships(Relationship **relationships, size_t count) {
    for (size_t i = 0; i < count; i++) {
        relationship_destroy(relationships[i]);
    }
}

AnalyzerResult thematic_return_detection_analyze(const void *input, const Context *context, const Value *config,
                                                  AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const ThematicReturnDetectionInput *analysis_input = (const ThematicReturnDetectionInput *)input;
    if (!validate_entries(analysis_input->entries, analysis_input->entry_count)) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    size_t entry_count = analysis_input->entry_count;
    size_t max_pair_count = entry_count < 2 ? 0 : (entry_count * (entry_count - 1)) / 2;

    Interpretation **interpretations = NULL;
    if (max_pair_count > 0) {
        interpretations = (Interpretation **)calloc(max_pair_count, sizeof(Interpretation *));
        if (interpretations == NULL) {
            return ANALYZER_ERR_ALLOC;
        }
    }

    size_t out_index = 0;
    for (size_t i = 0; i < entry_count; i++) {
        for (size_t j = i + 1; j < entry_count; j++) {
            const ThematicReturnDetectionEntry *earlier = &analysis_input->entries[i];
            const ThematicReturnDetectionEntry *later = &analysis_input->entries[j];
            if (!windows_match(earlier->window, later->window)) {
                continue;
            }

            const char *earlier_value = entity_id_value(earlier->theme_occurrence_id);
            const char *later_value = entity_id_value(later->theme_occurrence_id);

            EntityID *relationship_id;
            if (!make_id("relationship-thematic-return", later_value, earlier_value, &relationship_id)) {
                destroy_partial_interpretations(interpretations, out_index);
                free(interpretations);
                return ANALYZER_ERR_ALLOC;
            }
            EntityID *interpretation_id;
            if (!make_id("interpretation-thematic-return", later_value, earlier_value, &interpretation_id)) {
                entity_id_destroy(relationship_id);
                destroy_partial_interpretations(interpretations, out_index);
                free(interpretations);
                return ANALYZER_ERR_ALLOC;
            }

            ThematicReturnDetectionClaim claim_content;
            memset(&claim_content, 0, sizeof(claim_content));
            snprintf(claim_content.later_theme_occurrence_id, sizeof(claim_content.later_theme_occurrence_id), "%s",
                      later_value);
            snprintf(claim_content.earlier_theme_occurrence_id, sizeof(claim_content.earlier_theme_occurrence_id),
                      "%s", earlier_value);

            Value *claim;
            if (value_create("thematic-return-detection-claim", &claim_content, sizeof(claim_content), &claim) !=
                VALUE_OK) {
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
                                       "ThematicReturnDetection", "contour/rhythm exact-match thematic return",
                                       INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
                INTERPRETATION_OK) {
                destroy_partial_interpretations(interpretations, out_index);
                free(interpretations);
                return ANALYZER_ERR_ANALYSIS_FAILED;
            }
            interpretations[out_index] = interpretation;
            out_index++;
        }
    }

    EntityID *result_id;
    if (entity_id_create("analysis-result-thematic-return-detection", &result_id) != ENTITY_ID_OK) {
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

const Analyzer THEMATIC_RETURN_DETECTION_ANALYZER = {"ThematicReturnDetection", thematic_return_detection_analyze};

ThematicReturnDetectionRelationshipResult thematic_return_detection_build_relationships(
    const ThematicReturnDetectionEntry *entries, size_t entry_count, Relationship ***out_relationships,
    size_t *out_relationship_count) {
    if (entry_count > 0 && entries == NULL) {
        return THEMATIC_RETURN_DETECTION_REL_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (!valid_entry_shape(&entries[i])) {
            return THEMATIC_RETURN_DETECTION_REL_ERR_NULL_ENTRY;
        }
    }

    size_t max_pair_count = entry_count < 2 ? 0 : (entry_count * (entry_count - 1)) / 2;
    if (max_pair_count == 0) {
        *out_relationships = NULL;
        *out_relationship_count = 0;
        return THEMATIC_RETURN_DETECTION_REL_OK;
    }

    Relationship **relationships = (Relationship **)calloc(max_pair_count, sizeof(Relationship *));
    if (relationships == NULL) {
        return THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC;
    }

    size_t out_index = 0;
    for (size_t i = 0; i < entry_count; i++) {
        for (size_t j = i + 1; j < entry_count; j++) {
            const ThematicReturnDetectionEntry *earlier = &entries[i];
            const ThematicReturnDetectionEntry *later = &entries[j];
            if (!windows_match(earlier->window, later->window)) {
                continue;
            }

            const char *earlier_value = entity_id_value(earlier->theme_occurrence_id);
            const char *later_value = entity_id_value(later->theme_occurrence_id);

            EntityID *relationship_id;
            if (!make_id("relationship-thematic-return", later_value, earlier_value, &relationship_id)) {
                destroy_partial_relationships(relationships, out_index);
                free(relationships);
                return THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC;
            }
            EntityID *source_id;
            if (entity_id_create(later_value, &source_id) != ENTITY_ID_OK) {
                entity_id_destroy(relationship_id);
                destroy_partial_relationships(relationships, out_index);
                free(relationships);
                return THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC;
            }
            EntityID *target_id;
            if (entity_id_create(earlier_value, &target_id) != ENTITY_ID_OK) {
                entity_id_destroy(source_id);
                entity_id_destroy(relationship_id);
                destroy_partial_relationships(relationships, out_index);
                free(relationships);
                return THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC;
            }
            ProvenanceReference *provenance;
            if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
                entity_id_destroy(target_id);
                entity_id_destroy(source_id);
                entity_id_destroy(relationship_id);
                destroy_partial_relationships(relationships, out_index);
                free(relationships);
                return THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC;
            }

            Relationship *relationship;
            if (relationship_create(relationship_id, source_id, "resembles", target_id, NULL, NULL, NULL, NULL, NULL,
                                     provenance, &relationship) != RELATIONSHIP_OK) {
                destroy_partial_relationships(relationships, out_index);
                free(relationships);
                return THEMATIC_RETURN_DETECTION_REL_ERR_ALLOC;
            }
            relationships[out_index] = relationship;
            out_index++;
        }
    }

    *out_relationships = relationships;
    *out_relationship_count = out_index;
    return THEMATIC_RETURN_DETECTION_REL_OK;
}

void thematic_return_detection_destroy_relationships(Relationship **relationships, size_t relationship_count) {
    if (relationships == NULL) {
        return;
    }
    for (size_t i = 0; i < relationship_count; i++) {
        relationship_destroy(relationships[i]);
    }
    free(relationships);
}
