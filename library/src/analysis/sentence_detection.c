#include "analysis/sentence_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/sentence_continuation_matching.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#define SENTENCE_DETECTION_ID_MINT_BUFFER_SIZE 320

static bool valid_entry_shape(const SentenceDetectionEntry *entry) {
    if (entry->statement_occurrence_id == NULL || entry->repetition_occurrence_id == NULL ||
        entry->statement_window == NULL) {
        return false;
    }
    return entry->statement_window->contour != NULL && entry->statement_window->rhythm != NULL &&
           entry->statement_window->event_ids != NULL && entry->statement_window->event_id_count > 0;
}

static bool validate_entries(const SentenceDetectionEntry *entries, size_t entry_count) {
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

static bool qualifies(const SentenceDetectionEntry *entry) {
    if (entry->continuation_candidate == NULL) {
        return false;
    }
    return sentence_continuation_matching_is_continuation(entry->statement_window, entry->continuation_candidate);
}

static bool make_id(const char *prefix, const char *value_a, const char *value_b, EntityID **out) {
    char buffer[SENTENCE_DETECTION_ID_MINT_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s-%s", prefix, value_a, value_b);
    return entity_id_create(buffer, out) == ENTITY_ID_OK;
}

/* Deterministic id for the continuation's synthetic MotifOccurrence -- minted the same way
   MotifDetection mints an occurrence id, from the statement's id and the continuation
   candidate's first (anchor) event id. Reuses the MotifOccurrence type (no new
   Structure/Occurrence type): the id alone stands in for it here, the same way
   PeriodDetection's Relationship endpoints stand in for PhraseOccurrences by id. */
static bool make_continuation_occurrence_id(const char *statement_value, const MotifWindow *continuation_candidate,
                                             EntityID **out) {
    const char *anchor_value = entity_id_value(continuation_candidate->event_ids[0]);
    return make_id("occurrence-sentence-continuation", statement_value, anchor_value, out);
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

AnalyzerResult sentence_detection_analyze(const void *input, const Context *context, const Value *config,
                                           AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const SentenceDetectionInput *analysis_input = (const SentenceDetectionInput *)input;
    if (!validate_entries(analysis_input->entries, analysis_input->entry_count)) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    size_t entry_count = analysis_input->entry_count;
    Interpretation **interpretations = NULL;
    if (entry_count > 0) {
        interpretations = (Interpretation **)calloc(entry_count * 2, sizeof(Interpretation *));
        if (interpretations == NULL) {
            return ANALYZER_ERR_ALLOC;
        }
    }

    size_t out_index = 0;
    for (size_t i = 0; i < entry_count; i++) {
        const SentenceDetectionEntry *entry = &analysis_input->entries[i];
        if (!qualifies(entry)) {
            continue;
        }

        const char *statement_value = entity_id_value(entry->statement_occurrence_id);
        const char *repetition_value = entity_id_value(entry->repetition_occurrence_id);

        EntityID *continuation_id;
        if (!make_continuation_occurrence_id(statement_value, entry->continuation_candidate, &continuation_id)) {
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ALLOC;
        }
        const char *continuation_value = entity_id_value(continuation_id);

        SentenceDetectionClaim claim_content;
        memset(&claim_content, 0, sizeof(claim_content));
        snprintf(claim_content.statement_occurrence_id, sizeof(claim_content.statement_occurrence_id), "%s",
                  statement_value);
        snprintf(claim_content.repetition_occurrence_id, sizeof(claim_content.repetition_occurrence_id), "%s",
                  repetition_value);
        snprintf(claim_content.continuation_occurrence_id, sizeof(claim_content.continuation_occurrence_id), "%s",
                  continuation_value);

        entity_id_destroy(continuation_id);

        /* Link 1: repetition transformed_from statement. Link 2: continuation transformed_from repetition. */
        bool failed = false;
        for (size_t link = 0; link < 2 && !failed; link++) {
            const char *source_value = link == 0 ? repetition_value : continuation_value;
            const char *target_value = link == 0 ? statement_value : repetition_value;

            EntityID *interpretation_id;
            if (!make_id("interpretation-sentence", source_value, target_value, &interpretation_id)) {
                failed = true;
                break;
            }

            Value *claim;
            if (value_create("sentence-detection-claim", &claim_content, sizeof(claim_content), &claim) !=
                VALUE_OK) {
                entity_id_destroy(interpretation_id);
                failed = true;
                break;
            }

            Uncertainty *uncertainty;
            if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
                value_destroy(claim);
                entity_id_destroy(interpretation_id);
                failed = true;
                break;
            }

            ProvenanceReference *provenance;
            if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
                uncertainty_destroy(uncertainty);
                value_destroy(claim);
                entity_id_destroy(interpretation_id);
                failed = true;
                break;
            }

            EntityID *target_id;
            if (!make_id("relationship-sentence", source_value, target_value, &target_id)) {
                provenance_reference_destroy(provenance);
                uncertainty_destroy(uncertainty);
                value_destroy(claim);
                entity_id_destroy(interpretation_id);
                failed = true;
                break;
            }

            Interpretation *interpretation;
            if (interpretation_create(interpretation_id, target_id, claim, NULL, NULL, NULL, uncertainty,
                                       "SentenceDetection", "statement-repetition-continuation chaining",
                                       INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
                INTERPRETATION_OK) {
                failed = true;
                break;
            }
            interpretations[out_index] = interpretation;
            out_index++;
        }

        if (failed) {
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            return ANALYZER_ERR_ALLOC;
        }
    }

    EntityID *result_id;
    if (entity_id_create("analysis-result-sentence-detection", &result_id) != ENTITY_ID_OK) {
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

const Analyzer SENTENCE_DETECTION_ANALYZER = {"SentenceDetection", sentence_detection_analyze};

SentenceDetectionRelationshipResult sentence_detection_build_relationships(const SentenceDetectionEntry *entries,
                                                                            size_t entry_count,
                                                                            Relationship ***out_relationships,
                                                                            size_t *out_relationship_count) {
    if (entry_count > 0 && entries == NULL) {
        return SENTENCE_DETECTION_REL_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (!valid_entry_shape(&entries[i])) {
            return SENTENCE_DETECTION_REL_ERR_NULL_ENTRY;
        }
    }

    if (entry_count == 0) {
        *out_relationships = NULL;
        *out_relationship_count = 0;
        return SENTENCE_DETECTION_REL_OK;
    }

    Relationship **relationships = (Relationship **)calloc(entry_count * 2, sizeof(Relationship *));
    if (relationships == NULL) {
        return SENTENCE_DETECTION_REL_ERR_ALLOC;
    }

    size_t out_index = 0;
    for (size_t i = 0; i < entry_count; i++) {
        const SentenceDetectionEntry *entry = &entries[i];
        if (!qualifies(entry)) {
            continue;
        }

        const char *statement_value = entity_id_value(entry->statement_occurrence_id);
        const char *repetition_value = entity_id_value(entry->repetition_occurrence_id);

        EntityID *continuation_id;
        if (!make_continuation_occurrence_id(statement_value, entry->continuation_candidate, &continuation_id)) {
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return SENTENCE_DETECTION_REL_ERR_ALLOC;
        }
        const char *continuation_value = entity_id_value(continuation_id);

        /* Link 1: repetition transformed_from statement. */
        bool link_failed = false;
        for (size_t link = 0; link < 2 && !link_failed; link++) {
            const char *source_value = link == 0 ? repetition_value : continuation_value;
            const char *target_value = link == 0 ? statement_value : repetition_value;

            EntityID *relationship_id;
            if (!make_id("relationship-sentence", source_value, target_value, &relationship_id)) {
                link_failed = true;
                break;
            }
            EntityID *source_id;
            if (entity_id_create(source_value, &source_id) != ENTITY_ID_OK) {
                entity_id_destroy(relationship_id);
                link_failed = true;
                break;
            }
            EntityID *target_id;
            if (entity_id_create(target_value, &target_id) != ENTITY_ID_OK) {
                entity_id_destroy(source_id);
                entity_id_destroy(relationship_id);
                link_failed = true;
                break;
            }
            ProvenanceReference *provenance;
            if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
                entity_id_destroy(target_id);
                entity_id_destroy(source_id);
                entity_id_destroy(relationship_id);
                link_failed = true;
                break;
            }

            Relationship *relationship;
            if (relationship_create(relationship_id, source_id, "transformed_from", target_id, NULL, NULL, NULL, NULL,
                                     NULL, provenance, &relationship) != RELATIONSHIP_OK) {
                link_failed = true;
                break;
            }
            relationships[out_index] = relationship;
            out_index++;
        }

        entity_id_destroy(continuation_id);

        if (link_failed) {
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            return SENTENCE_DETECTION_REL_ERR_ALLOC;
        }
    }

    *out_relationships = relationships;
    *out_relationship_count = out_index;
    return SENTENCE_DETECTION_REL_OK;
}

void sentence_detection_destroy_relationships(Relationship **relationships, size_t relationship_count) {
    if (relationships == NULL) {
        return;
    }
    for (size_t i = 0; i < relationship_count; i++) {
        relationship_destroy(relationships[i]);
    }
    free(relationships);
}
