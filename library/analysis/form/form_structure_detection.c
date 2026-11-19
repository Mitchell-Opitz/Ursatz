#include "analysis/form/form_structure_detection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/identity/entity_id.h"

#define FORM_STRUCTURE_DETECTION_ID_MINT_BUFFER_SIZE 288

/* Only PAC and IAC achieve tonic resolution -- the only types treated as section-closing
   (structural) by this module's flagged default. Authentic-unspecified is deliberately not
   promoted. */
static bool is_structural_cadence(CadenceType cadence_type) {
    return cadence_type == CADENCE_TYPE_PERFECT_AUTHENTIC || cadence_type == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
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

static void destroy_partial_sections(Section **sections, size_t count) {
    for (size_t i = 0; i < count; i++) {
        section_destroy(sections[i]);
    }
}

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

FormStructureDetectionSectionResult form_structure_detection_order(const FormStructureDetectionEntry *entries, size_t entry_count,
                                                                    FormStructureOrder **out_pairs, size_t *out_pair_count) {
    *out_pairs = NULL;
    *out_pair_count = 0;
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
    free(boundaries);

    size_t pair_count = boundary_count < 2 ? 0 : boundary_count - 1;
    if (pair_count == 0) {
        return FORM_STRUCTURE_DETECTION_OK;
    }
    FormStructureOrder *pairs = (FormStructureOrder *)calloc(pair_count, sizeof(FormStructureOrder));
    if (pairs == NULL) {
        return FORM_STRUCTURE_DETECTION_ERR_ALLOC;
    }
    for (size_t s = 0; s < pair_count; s++) {
        pairs[s].earlier = s;
        pairs[s].later = s + 1;
    }
    *out_pairs = pairs;
    *out_pair_count = pair_count;
    return FORM_STRUCTURE_DETECTION_OK;
}
