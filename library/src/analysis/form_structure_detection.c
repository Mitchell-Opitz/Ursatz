#include "analysis/form_structure_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"

#define FORM_STRUCTURE_DETECTION_ID_MINT_BUFFER_SIZE 288

/* Only PAC and IAC achieve tonic resolution -- the only types treated as section-closing
   (structural) by this module's flagged default. Authentic-unspecified is deliberately not
   promoted. */
static bool is_structural_cadence(CadenceType cadence_type) {
    return cadence_type == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence_type == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
}

static bool validate_entries(const FormStructureDetectionEntry *entries, size_t entry_count) {
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

/*
 * Computes the boundary index (inclusive) closing each identified Section, in
 * chronological order. A boundary at index i means entries[0..i] (or, for a
 * later section, entries[previous_boundary+1..i]) form a Section closed by
 * entries[i]'s structural cadence. Only indices strictly before the last entry
 * are consulted (a structural cadence on the final entry cannot open a further
 * section). Returns NULL with *out_count == 0 (a valid, non-error outcome) when
 * no structural cadence is found.
 */
static bool compute_boundaries(const FormStructureDetectionEntry *entries, size_t entry_count, size_t **out_boundaries,
                                size_t *out_count) {
    size_t candidate_count = entry_count < 2 ? 0 : entry_count - 1;
    size_t *boundaries = candidate_count > 0 ? (size_t *)malloc(sizeof(size_t) * candidate_count) : NULL;
    if (candidate_count > 0 && boundaries == NULL) {
        return false;
    }

    size_t found = 0;
    for (size_t i = 0; i + 1 < entry_count; i++) {
        if (is_structural_cadence(entries[i].cadence_type)) {
            boundaries[found] = i;
            found++;
        }
    }

    if (found == 0) {
        free(boundaries);
        *out_boundaries = NULL;
        *out_count = 0;
        return true;
    }

    *out_boundaries = boundaries;
    *out_count = found;
    return true;
}

static bool make_id(const char *prefix, const char *value_a, const char *value_b, EntityID **out) {
    char buffer[FORM_STRUCTURE_DETECTION_ID_MINT_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s-%s-%s", prefix, value_a, value_b);
    return entity_id_create(buffer, out) == ENTITY_ID_OK;
}

static bool section_span_id(const FormStructureDetectionEntry *entries, size_t start, size_t end, EntityID **out) {
    const char *first_value = entity_id_value(phrase_occurrence_id(entries[start].phrase_occurrence));
    const char *last_value = entity_id_value(phrase_occurrence_id(entries[end].phrase_occurrence));
    return make_id("section-formstructure", first_value, last_value, out);
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

static void destroy_partial_sections(Section **sections, size_t count) {
    for (size_t i = 0; i < count; i++) {
        section_destroy(sections[i]);
    }
}

AnalyzerResult form_structure_detection_analyze(const void *input, const Context *context, const Value *config,
                                                 AnalysisResult **out) {
    (void)context;
    (void)config;

    if (input == NULL) {
        return ANALYZER_ERR_NULL_INPUT;
    }
    if (out == NULL) {
        return ANALYZER_ERR_NULL_OUT;
    }

    const FormStructureDetectionInput *analysis_input = (const FormStructureDetectionInput *)input;
    if (!validate_entries(analysis_input->entries, analysis_input->entry_count)) {
        return ANALYZER_ERR_NULL_INPUT;
    }

    size_t *boundaries;
    size_t boundary_count;
    if (!compute_boundaries(analysis_input->entries, analysis_input->entry_count, &boundaries, &boundary_count)) {
        return ANALYZER_ERR_ALLOC;
    }

    Interpretation **interpretations = NULL;
    size_t link_count = boundary_count == 0 ? 0 : boundary_count - 1;
    if (link_count > 0) {
        interpretations = (Interpretation **)calloc(link_count, sizeof(Interpretation *));
        if (interpretations == NULL) {
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }
    }

    size_t out_index = 0;
    size_t previous_start = 0;
    for (size_t s = 0; s + 1 < boundary_count; s++) {
        size_t earlier_start = previous_start;
        size_t earlier_end = boundaries[s];
        size_t later_start = boundaries[s] + 1;
        size_t later_end = boundaries[s + 1];
        previous_start = later_start;

        EntityID *earlier_section_id;
        if (!section_span_id(analysis_input->entries, earlier_start, earlier_end, &earlier_section_id)) {
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *later_section_id;
        if (!section_span_id(analysis_input->entries, later_start, later_end, &later_section_id)) {
            entity_id_destroy(earlier_section_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }

        const char *earlier_value = entity_id_value(earlier_section_id);
        const char *later_value = entity_id_value(later_section_id);

        EntityID *relationship_id;
        if (!make_id("relationship-formstructure-order", earlier_value, later_value, &relationship_id)) {
            entity_id_destroy(later_section_id);
            entity_id_destroy(earlier_section_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }
        EntityID *interpretation_id;
        if (!make_id("interpretation-formstructure-order", earlier_value, later_value, &interpretation_id)) {
            entity_id_destroy(relationship_id);
            entity_id_destroy(later_section_id);
            entity_id_destroy(earlier_section_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }

        FormStructureDetectionClaim claim_content;
        memset(&claim_content, 0, sizeof(claim_content));
        snprintf(claim_content.earlier_section_id, sizeof(claim_content.earlier_section_id), "%s", earlier_value);
        snprintf(claim_content.later_section_id, sizeof(claim_content.later_section_id), "%s", later_value);
        claim_content.earlier_section_closing_cadence_type = analysis_input->entries[earlier_end].cadence_type;

        entity_id_destroy(later_section_id);
        entity_id_destroy(earlier_section_id);

        Value *claim;
        if (value_create("form-structure-detection-claim", &claim_content, sizeof(claim_content), &claim) !=
            VALUE_OK) {
            entity_id_destroy(interpretation_id);
            entity_id_destroy(relationship_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }

        Uncertainty *uncertainty;
        if (uncertainty_create_scalar_confidence(1, 1, &uncertainty) != UNCERTAINTY_OK) {
            value_destroy(claim);
            entity_id_destroy(interpretation_id);
            entity_id_destroy(relationship_id);
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
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
            free(boundaries);
            return ANALYZER_ERR_ALLOC;
        }

        Interpretation *interpretation;
        if (interpretation_create(interpretation_id, relationship_id, claim, NULL, NULL, NULL, uncertainty,
                                   "FormStructureDetection", "structural-cadence section boundary ordering",
                                   INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
            INTERPRETATION_OK) {
            destroy_partial_interpretations(interpretations, out_index);
            free(interpretations);
            free(boundaries);
            return ANALYZER_ERR_ANALYSIS_FAILED;
        }
        interpretations[out_index] = interpretation;
        out_index++;
    }

    free(boundaries);

    EntityID *result_id;
    if (entity_id_create("analysis-result-form-structure-detection", &result_id) != ENTITY_ID_OK) {
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

const Analyzer FORM_STRUCTURE_DETECTION_ANALYZER = {"FormStructureDetection", form_structure_detection_analyze};

FormStructureDetectionSectionResult form_structure_detection_build_sections(
    const FormStructureDetectionEntry *entries, size_t entry_count, Section ***out_sections,
    size_t *out_section_count) {
    if (entry_count > 0 && entries == NULL) {
        return FORM_STRUCTURE_DETECTION_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].phrase_occurrence == NULL) {
            return FORM_STRUCTURE_DETECTION_ERR_NULL_ENTRY;
        }
    }

    size_t *boundaries;
    size_t boundary_count;
    if (!compute_boundaries(entries, entry_count, &boundaries, &boundary_count)) {
        return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
    }
    if (boundary_count == 0) {
        *out_sections = NULL;
        *out_section_count = 0;
        return FORM_STRUCTURE_DETECTION_OK;
    }

    Section **sections = (Section **)calloc(boundary_count, sizeof(Section *));
    if (sections == NULL) {
        free(boundaries);
        return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
    }

    size_t out_index = 0;
    size_t previous_start = 0;
    for (size_t s = 0; s < boundary_count; s++) {
        size_t start = previous_start;
        size_t end = boundaries[s];
        previous_start = end + 1;

        size_t span_count = end - start + 1;
        EntityID **phrase_ids = (EntityID **)calloc(span_count, sizeof(EntityID *));
        if (phrase_ids == NULL) {
            destroy_partial_sections(sections, out_index);
            free(sections);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
        }

        bool copy_failed = false;
        size_t copied = 0;
        for (size_t i = start; i <= end; i++) {
            const char *phrase_value = entity_id_value(phrase_occurrence_phrase_id(entries[i].phrase_occurrence));
            if (entity_id_create(phrase_value, &phrase_ids[copied]) != ENTITY_ID_OK) {
                copy_failed = true;
                break;
            }
            copied++;
        }
        if (copy_failed) {
            for (size_t i = 0; i < copied; i++) {
                entity_id_destroy(phrase_ids[i]);
            }
            free(phrase_ids);
            destroy_partial_sections(sections, out_index);
            free(sections);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
        }

        EntityID *section_id;
        if (!section_span_id(entries, start, end, &section_id)) {
            for (size_t i = 0; i < span_count; i++) {
                entity_id_destroy(phrase_ids[i]);
            }
            free(phrase_ids);
            destroy_partial_sections(sections, out_index);
            free(sections);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
        }

        Section *section;
        if (section_create(section_id, phrase_ids, span_count, &section) != SECTION_OK) {
            for (size_t i = 0; i < span_count; i++) {
                entity_id_destroy(phrase_ids[i]);
            }
            free(phrase_ids);
            entity_id_destroy(section_id);
            destroy_partial_sections(sections, out_index);
            free(sections);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
        }
        free(phrase_ids);
        sections[out_index] = section;
        out_index++;
    }

    free(boundaries);
    *out_sections = sections;
    *out_section_count = out_index;
    return FORM_STRUCTURE_DETECTION_OK;
}

void form_structure_detection_destroy_sections(Section **sections, size_t section_count) {
    if (sections == NULL) {
        return;
    }
    for (size_t i = 0; i < section_count; i++) {
        section_destroy(sections[i]);
    }
    free(sections);
}

FormStructureDetectionRelationshipResult form_structure_detection_build_relationships(
    const FormStructureDetectionEntry *entries, size_t entry_count, Relationship ***out_relationships,
    size_t *out_relationship_count) {
    if (entry_count > 0 && entries == NULL) {
        return FORM_STRUCTURE_DETECTION_REL_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].phrase_occurrence == NULL) {
            return FORM_STRUCTURE_DETECTION_REL_ERR_NULL_ENTRY;
        }
    }

    size_t *boundaries;
    size_t boundary_count;
    if (!compute_boundaries(entries, entry_count, &boundaries, &boundary_count)) {
        return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
    }

    size_t link_count = boundary_count < 2 ? 0 : boundary_count - 1;
    if (link_count == 0) {
        free(boundaries);
        *out_relationships = NULL;
        *out_relationship_count = 0;
        return FORM_STRUCTURE_DETECTION_REL_OK;
    }

    Relationship **relationships = (Relationship **)calloc(link_count, sizeof(Relationship *));
    if (relationships == NULL) {
        free(boundaries);
        return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
    }

    size_t out_index = 0;
    size_t previous_start = 0;
    for (size_t s = 0; s + 1 < boundary_count; s++) {
        size_t earlier_start = previous_start;
        size_t earlier_end = boundaries[s];
        size_t later_start = boundaries[s] + 1;
        size_t later_end = boundaries[s + 1];
        previous_start = later_start;

        EntityID *earlier_section_id;
        if (!section_span_id(entries, earlier_start, earlier_end, &earlier_section_id)) {
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
        }
        EntityID *later_section_id;
        if (!section_span_id(entries, later_start, later_end, &later_section_id)) {
            entity_id_destroy(earlier_section_id);
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
        }

        const char *earlier_value = entity_id_value(earlier_section_id);
        const char *later_value = entity_id_value(later_section_id);

        EntityID *relationship_id;
        if (!make_id("relationship-formstructure-order", earlier_value, later_value, &relationship_id)) {
            entity_id_destroy(later_section_id);
            entity_id_destroy(earlier_section_id);
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
        }
        EntityID *source_id = earlier_section_id;
        EntityID *target_id = later_section_id;

        ProvenanceReference *provenance;
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            entity_id_destroy(target_id);
            entity_id_destroy(source_id);
            entity_id_destroy(relationship_id);
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
        }

        Relationship *relationship;
        if (relationship_create(relationship_id, source_id, "precedes", target_id, NULL, NULL, NULL, NULL, NULL,
                                 provenance, &relationship) != RELATIONSHIP_OK) {
            destroy_partial_relationships(relationships, out_index);
            free(relationships);
            free(boundaries);
            return FORM_STRUCTURE_DETECTION_REL_ERR_ALLOC;
        }
        relationships[out_index] = relationship;
        out_index++;
    }

    free(boundaries);
    *out_relationships = relationships;
    *out_relationship_count = out_index;
    return FORM_STRUCTURE_DETECTION_REL_OK;
}

void form_structure_detection_destroy_relationships(Relationship **relationships, size_t relationship_count) {
    if (relationships == NULL) {
        return;
    }
    for (size_t i = 0; i < relationship_count; i++) {
        relationship_destroy(relationships[i]);
    }
    free(relationships);
}
