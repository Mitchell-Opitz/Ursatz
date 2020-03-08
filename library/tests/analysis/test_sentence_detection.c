#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/sentence_detection.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "pitch/pitch_identity.h"
#include "relationship/relationship.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "time/duration.h"

/*
 * Behavioral-contract test: NULL input is rejected without touching *out,
 * and a valid input with no qualifying entries yields an empty
 * AnalysisResult / relationship set.
 *
 * Analysis-regression test: one entry with a qualifying continuation
 * (shortened, direction/duration prefix of the statement) produces the
 * chained "repetition transformed_from statement" / "continuation
 * transformed_from repetition" Relationship pair and matching
 * Interpretation pair; a second entry with no continuation candidate
 * produces no output for that entry (requirement 4).
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *identity;
    assert(pitch_identity_create(token, 2, &identity) == PITCH_IDENTITY_OK);
    return identity;
}

static Contour *make_contour_3_up_down(const char *p0, const char *p1, const char *p2) {
    PitchIdentity *pitch_ids[3] = {make_pitch(p0), make_pitch(p1), make_pitch(p2)};
    ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN};
    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);
    return contour;
}

static Contour *make_contour_2_up(const char *p0, const char *p1) {
    PitchIdentity *pitch_ids[2] = {make_pitch(p0), make_pitch(p1)};
    ContourDirection directions[1] = {CONTOUR_DIRECTION_UP};
    Contour *contour;
    assert(contour_create(pitch_ids, 2, directions, 1, &contour) == CONTOUR_OK);
    return contour;
}

static RhythmicPattern *make_rhythm(size_t count) {
    Duration *durations[3];
    for (size_t i = 0; i < count; i++) {
        assert(duration_create(1, 4, &durations[i]) == DURATION_OK);
    }
    RhythmicPattern *pattern;
    assert(rhythmic_pattern_create(durations, count, &pattern) == RHYTHMIC_PATTERN_OK);
    return pattern;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(sentence_detection_analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(SENTENCE_DETECTION_ANALYZER.name, "SentenceDetection") == 0);

    /* Behavioral contract: 0 entries is valid and empty. */
    SentenceDetectionInput empty_input = {NULL, 0};
    AnalysisResult *empty_result;
    assert(sentence_detection_analyze(&empty_input, NULL, NULL, &empty_result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(empty_result) == 0);
    analysis_result_destroy(empty_result);

    Relationship **empty_relationships;
    size_t empty_relationship_count;
    assert(sentence_detection_build_relationships(NULL, 0, &empty_relationships, &empty_relationship_count) ==
           SENTENCE_DETECTION_REL_OK);
    assert(empty_relationships == NULL);
    assert(empty_relationship_count == 0);

    EntityID *statement_occurrence_id = make_id("occurrence-motif-detection-motif-0-note-s-0");
    EntityID *repetition_occurrence_id = make_id("occurrence-motif-detection-motif-0-note-r-0");

    EntityID *statement_events[3] = {make_id("note-s-0"), make_id("note-s-1"), make_id("note-s-2")};
    Contour *statement_contour = make_contour_3_up_down("c\0", "e\0", "d\0");
    RhythmicPattern *statement_rhythm = make_rhythm(3);
    MotifWindow statement_window = {statement_contour, statement_rhythm, (const EntityID *const *)statement_events,
                                     3};

    /* Qualifying continuation: shortened, direction/duration prefix of the statement, transposed. */
    EntityID *continuation_events[2] = {make_id("note-c-0"), make_id("note-c-1")};
    Contour *continuation_contour = make_contour_2_up("g\0", "b\0");
    RhythmicPattern *continuation_rhythm = make_rhythm(2);
    MotifWindow continuation_window = {continuation_contour, continuation_rhythm,
                                        (const EntityID *const *)continuation_events, 2};

    SentenceDetectionEntry qualifying_entry = {statement_occurrence_id, repetition_occurrence_id, &statement_window,
                                                &continuation_window};
    /* No candidate continuation material for this pair -- a legitimate outcome (requirement 4). */
    SentenceDetectionEntry non_qualifying_entry = {statement_occurrence_id, repetition_occurrence_id,
                                                    &statement_window, NULL};

    SentenceDetectionEntry entries[2] = {qualifying_entry, non_qualifying_entry};
    SentenceDetectionInput input = {entries, 2};

    AnalysisResult *result;
    assert(sentence_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 2);

    const Interpretation *interpretation_0 = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation_0)),
                   "relationship-sentence-occurrence-motif-detection-motif-0-note-r-0-occurrence-motif-detection-"
                   "motif-0-note-s-0") == 0);
    size_t claim_size;
    const SentenceDetectionClaim *claim_0 =
        (const SentenceDetectionClaim *)value_content(interpretation_claim(interpretation_0), &claim_size);
    assert(claim_size == sizeof(SentenceDetectionClaim));
    assert(strcmp(claim_0->statement_occurrence_id, "occurrence-motif-detection-motif-0-note-s-0") == 0);
    assert(strcmp(claim_0->repetition_occurrence_id, "occurrence-motif-detection-motif-0-note-r-0") == 0);
    assert(strcmp(claim_0->continuation_occurrence_id,
                   "occurrence-sentence-continuation-occurrence-motif-detection-motif-0-note-s-0-note-c-0") == 0);
    const Uncertainty *uncertainty_0 = interpretation_uncertainty(interpretation_0);
    assert(uncertainty_scalar_confidence_numerator(uncertainty_0) == 1);
    assert(uncertainty_scalar_confidence_denominator(uncertainty_0) == 1);
    assert(strcmp(interpretation_author(interpretation_0), "SentenceDetection") == 0);

    const Interpretation *interpretation_1 = analysis_result_interpretation_at(result, 1);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation_1)),
                   "relationship-sentence-occurrence-sentence-continuation-occurrence-motif-detection-motif-0-note-"
                   "s-0-note-c-0-occurrence-motif-detection-motif-0-note-r-0") == 0);

    analysis_result_destroy(result);

    Relationship **relationships;
    size_t relationship_count;
    assert(sentence_detection_build_relationships(entries, 2, &relationships, &relationship_count) ==
           SENTENCE_DETECTION_REL_OK);
    assert(relationship_count == 2);

    assert(strcmp(entity_id_value(relationship_source_id(relationships[0])),
                   "occurrence-motif-detection-motif-0-note-r-0") == 0);
    assert(strcmp(relationship_predicate_name(relationships[0]), "transformed_from") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(relationships[0])),
                   "occurrence-motif-detection-motif-0-note-s-0") == 0);

    assert(strcmp(entity_id_value(relationship_source_id(relationships[1])),
                   "occurrence-sentence-continuation-occurrence-motif-detection-motif-0-note-s-0-note-c-0") == 0);
    assert(strcmp(relationship_predicate_name(relationships[1]), "transformed_from") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(relationships[1])),
                   "occurrence-motif-detection-motif-0-note-r-0") == 0);

    sentence_detection_destroy_relationships(relationships, relationship_count);

    entity_id_destroy(statement_occurrence_id);
    entity_id_destroy(repetition_occurrence_id);
    for (size_t i = 0; i < 3; i++) {
        entity_id_destroy(statement_events[i]);
    }
    for (size_t i = 0; i < 2; i++) {
        entity_id_destroy(continuation_events[i]);
    }
    contour_destroy(statement_contour);
    contour_destroy(continuation_contour);
    rhythmic_pattern_destroy(statement_rhythm);
    rhythmic_pattern_destroy(continuation_rhythm);

    printf("test_sentence_detection: ok\n");
    return 0;
}
