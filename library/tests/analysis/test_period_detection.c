#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/period_detection.h"
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
 * and fewer than two entries (0 or 1) is a valid, empty AnalysisResult /
 * relationship set (no consecutive pair exists).
 *
 * Analysis-regression test: a four-phrase sequence exercises the
 * cadence-strength pairing rule (a real HC->PAC pair, a wrong-direction
 * PAC->HC non-pair, an equal-strength non-pair, and a pair touching
 * "unclassified" skipped entirely), and pins the resulting Relationship
 * (source/predicate/target) and Interpretation (target, claim, confidence)
 * shape against a future refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static ProvenanceReference *make_absent_provenance(void) {
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    return provenance;
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
    assert(period_detection_analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(PERIOD_DETECTION_ANALYZER.name, "PeriodDetection") == 0);

    /* Behavioral contract: 0 or 1 entries is valid and empty (no consecutive pair). */
    PeriodDetectionInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(period_detection_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    Relationship **empty_relationships;
    size_t empty_relationship_count;
    assert(period_detection_build_relationships(NULL, 0, &empty_relationships, &empty_relationship_count) ==
           PERIOD_DETECTION_REL_OK);
    assert(empty_relationships == NULL);
    assert(empty_relationship_count == 0);

    PhraseOccurrence *phrase0 = make_phrase_occurrence("phrase-occ-0", "phrase-0", "note-0");
    PeriodDetectionEntry single_entry[1] = {{phrase0, "half cadence (HC)"}};
    PeriodDetectionInput single_input = {single_entry, 1};
    AnalysisResult *single_result;
    assert(period_detection_analyze(&single_input, NULL, NULL, &single_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(single_result) == 0);
    analysis_result_destroy(single_result);

    /*
     * Sequence:
     *   entries[0]: HC   -> entries[1]: PAC                  -- period pair (weaker -> stronger)
     *   entries[1]: PAC  -> entries[2]: HC                   -- not a pair (wrong direction)
     *   entries[2]: HC   -> entries[3]: "unclassified"       -- not a pair (unclassified skipped)
     */
    PhraseOccurrence *phrase1 = make_phrase_occurrence("phrase-occ-1", "phrase-1", "note-1");
    PhraseOccurrence *phrase2 = make_phrase_occurrence("phrase-occ-2", "phrase-2", "note-2");
    PhraseOccurrence *phrase3 = make_phrase_occurrence("phrase-occ-3", "phrase-3", "note-3");

    PeriodDetectionEntry entries[4] = {
        {phrase0, "half cadence (HC)"},
        {phrase1, "authentic (PAC)"},
        {phrase2, "half cadence (HC)"},
        {phrase3, "unclassified"},
    };
    PeriodDetectionInput input = {entries, 4};

    AnalysisResult *result;
    assert(period_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)),
                   "relationship-period-phrase-occ-0-phrase-occ-1") == 0);
    size_t claim_size;
    const PeriodDetectionClaim *claim =
        (const PeriodDetectionClaim *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == sizeof(PeriodDetectionClaim));
    assert(strcmp(claim->antecedent_phrase_occurrence_id, "phrase-occ-0") == 0);
    assert(strcmp(claim->consequent_phrase_occurrence_id, "phrase-occ-1") == 0);
    assert(strcmp(claim->antecedent_cadence_type, "half cadence (HC)") == 0);
    assert(strcmp(claim->consequent_cadence_type, "authentic (PAC)") == 0);
    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 1);
    assert(uncertainty_scalar_confidence_denominator(uncertainty) == 1);
    assert(strcmp(interpretation_author(interpretation), "PeriodDetection") == 0);

    analysis_result_destroy(result);

    /* period_detection_build_relationships mirrors the same single pair, with matching id. */
    Relationship **relationships;
    size_t relationship_count;
    assert(period_detection_build_relationships(entries, 4, &relationships, &relationship_count) ==
           PERIOD_DETECTION_REL_OK);
    assert(relationship_count == 1);
    assert(strcmp(entity_id_value(relationship_id(relationships[0])), "relationship-period-phrase-occ-0-phrase-occ-1") ==
           0);
    assert(strcmp(entity_id_value(relationship_source_id(relationships[0])), "phrase-occ-0") == 0);
    assert(strcmp(relationship_predicate_name(relationships[0]), "precedes") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(relationships[0])), "phrase-occ-1") == 0);
    period_detection_destroy_relationships(relationships, relationship_count);

    phrase_occurrence_destroy(phrase0);
    phrase_occurrence_destroy(phrase1);
    phrase_occurrence_destroy(phrase2);
    phrase_occurrence_destroy(phrase3);

    printf("test_period_detection: ok\n");
    return 0;
}
