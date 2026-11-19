#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/form/form_structure_detection.h"
#include "model/containers/section.h"
#include "model/core/occurrence.h"
#include "model/core/provenance_reference.h"
#include "model/identity/entity_id.h"
#include "theory/vocabulary/cadence_type.h"
#include "model/structure/phrase_occurrence.h"

/*
 * Behavioral-contract test: NULL input is rejected without touching *out,
 * and an input with no structural cadence among its entries is a valid,
 * empty Section / order set (requirement 4 -- no
 * detected form structure is legitimate, never forced).
 *
 * Analysis-regression test: a five-phrase sequence with a structural
 * (authentic PAC) cadence at entries[1] and another (authentic IAC) at
 * entries[3] splits into three Sections -- [0,1], [2,3], [4] -- of which
 * only the first two are closed by a structural cadence and thus
 * reported (entries[4] trails with no closing cadence, so it is not
 * turned into a Section of its own, skip-don't-guess). Pins the resulting
 * Section (phrase_ids) and order
 * shape against a future refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static PhraseOccurrence *make_phrase_occurrence(const char *occurrence_id_value, const char *phrase_id_value,
                                                 const char *event_id_value) {
    Occurrence *occurrence;
    assert(occurrence_create(make_id(occurrence_id_value), make_id(phrase_id_value), &occurrence) == OCCURRENCE_OK);
    EntityID **region_event_ids = (EntityID **)malloc(sizeof(EntityID *));
    region_event_ids[0] = make_id(event_id_value);
    PhraseOccurrence *phrase_occurrence;
    assert(phrase_occurrence_create(occurrence, region_event_ids, 1, &phrase_occurrence) == PHRASE_OCCURRENCE_OK);
    return phrase_occurrence;
}

int main(void) {
    /* Behavioral contract: 0 entries is valid and empty. */
    Section **empty_sections;
    size_t empty_section_count;
    assert(form_structure_detection_build_sections(NULL, 0, &empty_sections, &empty_section_count) ==
           FORM_STRUCTURE_DETECTION_OK);
    assert(empty_sections == NULL);
    assert(empty_section_count == 0);

    FormStructureOrder *empty_order;
    size_t empty_order_count;
    assert(form_structure_detection_order(NULL, 0, &empty_order, &empty_order_count) == FORM_STRUCTURE_DETECTION_OK);
    assert(empty_order == NULL);
    assert(empty_order_count == 0);

    /* Requirement 4: no structural cadence anywhere -> no boundary found, no output. */
    PhraseOccurrence *only_phrase = make_phrase_occurrence("phrase-occ-only", "phrase-only", "note-only");
    FormStructureDetectionEntry no_boundary_entries[2] = {
        {only_phrase, CADENCE_TYPE_HALF},
        {only_phrase, CADENCE_TYPE_PLAGAL},
    };
    Section **none;
    size_t none_count;
    assert(form_structure_detection_build_sections(no_boundary_entries, 2, &none, &none_count) == FORM_STRUCTURE_DETECTION_OK);
    assert(none == NULL && none_count == 0);

    /* An unspecified authentic cadence is not promoted to structural, and neither is an unknown one. */
    FormStructureDetectionEntry unspecified_entries[2] = {
        {only_phrase, CADENCE_TYPE_AUTHENTIC_UNSPECIFIED},
        {only_phrase, CADENCE_TYPE_UNKNOWN},
    };
    assert(form_structure_detection_build_sections(unspecified_entries, 2, &none, &none_count) == FORM_STRUCTURE_DETECTION_OK);
    assert(none == NULL && none_count == 0);
    phrase_occurrence_destroy(only_phrase);

    /*
     * Sequence:
     *   entries[0]: HC
     *   entries[1]: authentic (PAC)   -- structural: closes Section [0,1]
     *   entries[2]: deceptive
     *   entries[3]: authentic (IAC)   -- structural: closes Section [2,3]
     *   entries[4]: half cadence (HC) -- trailing, never closed -> not a Section
     */
    PhraseOccurrence *phrase0 = make_phrase_occurrence("phrase-occ-0", "phrase-0", "note-0");
    PhraseOccurrence *phrase1 = make_phrase_occurrence("phrase-occ-1", "phrase-1", "note-1");
    PhraseOccurrence *phrase2 = make_phrase_occurrence("phrase-occ-2", "phrase-2", "note-2");
    PhraseOccurrence *phrase3 = make_phrase_occurrence("phrase-occ-3", "phrase-3", "note-3");
    PhraseOccurrence *phrase4 = make_phrase_occurrence("phrase-occ-4", "phrase-4", "note-4");

    FormStructureDetectionEntry entries[5] = {
        {phrase0, CADENCE_TYPE_HALF},
        {phrase1, CADENCE_TYPE_PERFECT_AUTHENTIC},
        {phrase2, CADENCE_TYPE_DECEPTIVE},
        {phrase3, CADENCE_TYPE_IMPERFECT_AUTHENTIC},
        {phrase4, CADENCE_TYPE_HALF},
    };

    /* form_structure_detection_build_sections mints the two closed Sections, in order. */
    Section **sections;
    size_t section_count;
    assert(form_structure_detection_build_sections(entries, 5, &sections, &section_count) ==
           FORM_STRUCTURE_DETECTION_OK);
    assert(section_count == 2);

    assert(strcmp(entity_id_value(section_id(sections[0])), "section-formstructure-phrase-occ-0-phrase-occ-1") == 0);
    assert(section_phrase_id_count(sections[0]) == 2);
    assert(strcmp(entity_id_value(section_phrase_id_at(sections[0], 0)), "phrase-0") == 0);
    assert(strcmp(entity_id_value(section_phrase_id_at(sections[0], 1)), "phrase-1") == 0);

    assert(strcmp(entity_id_value(section_id(sections[1])), "section-formstructure-phrase-occ-2-phrase-occ-3") == 0);
    assert(section_phrase_id_count(sections[1]) == 2);
    assert(strcmp(entity_id_value(section_phrase_id_at(sections[1], 0)), "phrase-2") == 0);
    assert(strcmp(entity_id_value(section_phrase_id_at(sections[1], 1)), "phrase-3") == 0);

    form_structure_detection_destroy_sections(sections, section_count);

    /* form_structure_detection_order gives the same single link: section 0 before section 1. */
    FormStructureOrder *order;
    size_t order_count;
    assert(form_structure_detection_order(entries, 5, &order, &order_count) == FORM_STRUCTURE_DETECTION_OK);
    assert(order_count == 1);
    assert(order[0].earlier == 0 && order[0].later == 1);
    free(order);

    phrase_occurrence_destroy(phrase0);
    phrase_occurrence_destroy(phrase1);
    phrase_occurrence_destroy(phrase2);
    phrase_occurrence_destroy(phrase3);
    phrase_occurrence_destroy(phrase4);

    printf("test_form_structure_detection: ok\n");
    return 0;
}
