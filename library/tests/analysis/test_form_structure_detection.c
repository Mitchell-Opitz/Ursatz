#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/form_structure_detection.h"
#include "containers/section.h"
#include "core/occurrence.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "relationship/relationship.h"
#include "structure/phrase_occurrence.h"

/*
 * Behavioral-contract test: NULL input is rejected without touching *out,
 * and an input with no structural cadence among its entries is a valid,
 * empty AnalysisResult / Section / Relationship set (requirement 4 -- no
 * detected form structure is legitimate, never forced).
 *
 * Analysis-regression test: a five-phrase sequence with a structural
 * (authentic PAC) cadence at entries[1] and another (authentic IAC) at
 * entries[3] splits into three Sections -- [0,1], [2,3], [4] -- of which
 * only the first two are closed by a structural cadence and thus
 * reported (entries[4] trails with no closing cadence, so it is not
 * turned into a Section of its own, matching PeriodDetection/
 * SentenceDetection's skip-don't-guess discipline). Pins the resulting
 * Section (phrase_ids) and Relationship/Interpretation ("precedes")
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
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(form_structure_detection_analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(FORM_STRUCTURE_DETECTION_ANALYZER.name, "FormStructureDetection") == 0);

    /* Behavioral contract: 0 entries is valid and empty. */
    FormStructureDetectionInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(form_structure_detection_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    Section **empty_sections;
    size_t empty_section_count;
    assert(form_structure_detection_build_sections(NULL, 0, &empty_sections, &empty_section_count) ==
           FORM_STRUCTURE_DETECTION_OK);
    assert(empty_sections == NULL);
    assert(empty_section_count == 0);

    Relationship **empty_relationships;
    size_t empty_relationship_count;
    assert(form_structure_detection_build_relationships(NULL, 0, &empty_relationships, &empty_relationship_count) ==
           FORM_STRUCTURE_DETECTION_REL_OK);
    assert(empty_relationships == NULL);
    assert(empty_relationship_count == 0);

    /* Requirement 4: no structural cadence anywhere -> no boundary found, no output. */
    PhraseOccurrence *only_phrase = make_phrase_occurrence("phrase-occ-only", "phrase-only", "note-only");
    FormStructureDetectionEntry no_boundary_entries[2] = {
        {only_phrase, "half cadence (HC)"},
        {only_phrase, "plagal"},
    };
    FormStructureDetectionInput no_boundary_input = {no_boundary_entries, 2};
    AnalysisResult *no_boundary_result;
    assert(form_structure_detection_analyze(&no_boundary_input, NULL, NULL, &no_boundary_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(no_boundary_result) == 0);
    analysis_result_destroy(no_boundary_result);
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
        {phrase0, "half cadence (HC)"}, {phrase1, "authentic (PAC)"}, {phrase2, "deceptive"},
        {phrase3, "authentic (IAC)"},   {phrase4, "half cadence (HC)"},
    };
    FormStructureDetectionInput input = {entries, 5};

    AnalysisResult *result;
    assert(form_structure_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)),
                   "relationship-formstructure-order-section-formstructure-phrase-occ-0-phrase-occ-1-"
                   "section-formstructure-phrase-occ-2-phrase-occ-3") == 0);
    size_t claim_size;
    const FormStructureDetectionClaim *claim =
        (const FormStructureDetectionClaim *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == sizeof(FormStructureDetectionClaim));
    assert(strcmp(claim->earlier_section_id, "section-formstructure-phrase-occ-0-phrase-occ-1") == 0);
    assert(strcmp(claim->later_section_id, "section-formstructure-phrase-occ-2-phrase-occ-3") == 0);
    assert(strcmp(claim->earlier_section_closing_cadence_type, "authentic (PAC)") == 0);
    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 1);
    assert(uncertainty_scalar_confidence_denominator(uncertainty) == 1);
    assert(strcmp(interpretation_author(interpretation), "FormStructureDetection") == 0);

    analysis_result_destroy(result);

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

    /* form_structure_detection_build_relationships mirrors the same single "precedes" link. */
    Relationship **relationships;
    size_t relationship_count;
    assert(form_structure_detection_build_relationships(entries, 5, &relationships, &relationship_count) ==
           FORM_STRUCTURE_DETECTION_REL_OK);
    assert(relationship_count == 1);
    assert(strcmp(entity_id_value(relationship_source_id(relationships[0])),
                   "section-formstructure-phrase-occ-0-phrase-occ-1") == 0);
    assert(strcmp(relationship_predicate_name(relationships[0]), "precedes") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(relationships[0])),
                   "section-formstructure-phrase-occ-2-phrase-occ-3") == 0);
    form_structure_detection_destroy_relationships(relationships, relationship_count);

    phrase_occurrence_destroy(phrase0);
    phrase_occurrence_destroy(phrase1);
    phrase_occurrence_destroy(phrase2);
    phrase_occurrence_destroy(phrase3);
    phrase_occurrence_destroy(phrase4);

    printf("test_form_structure_detection: ok\n");
    return 0;
}
