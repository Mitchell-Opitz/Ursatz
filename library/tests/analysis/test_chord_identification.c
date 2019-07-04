#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/chord_identification.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "semantics/sonority.h"
#include "theory/framework.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * Behavioral-contract test: ChordIdentification conforms to the
 * Analyzer interface (analysis/analyzer.h) -- NULL input, a NULL
 * sonority, a NULL framework, and an empty candidate set are all
 * rejected without touching *out -- and its output carries the
 * Interpretation/Evidence/Uncertainty architecture TDD Sec242
 * requires: a claim naming the best-fit candidate, an Uncertainty
 * holding the full candidate set (chord identification is ambiguous,
 * same as scale identification), and a referenced Evidence whose
 * source names the Framework the identification was made under.
 *
 * Analysis-regression test: a fixed two-candidate set ("C major triad"
 * 3/4, "A minor 6th (1st inv.)" 1/4) under a fixed Framework
 * ("common-practice-tonal-harmony") always yields the same top pick,
 * the same Evidence strength/source, and the same full candidate set
 * on the Interpretation's Uncertainty -- pinning ChordIdentification's
 * behavior against a future refactor.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Sonority *make_sonority(const char *event_id_value) {
    MusicalTime *start;
    assert(musical_time_create(0, 4, &start) == MUSICAL_TIME_OK);
    MusicalTime *end;
    assert(musical_time_create(1, 4, &end) == MUSICAL_TIME_OK);
    TimeSpan *time_window;
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &time_window) == TIME_SPAN_OK);
    musical_time_destroy(start);
    musical_time_destroy(end);

    EntityID *event_ids[1] = {make_id(event_id_value)};
    Sonority *sonority;
    assert(sonority_create(time_window, event_ids, 1, NULL, 0, SONORITY_EVENT_DOMAIN_SYMBOLIC, &sonority) ==
           SONORITY_OK);
    return sonority;
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(CHORD_IDENTIFICATION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(CHORD_IDENTIFICATION_ANALYZER.name, "ChordIdentification") == 0);

    Sonority *sonority = make_sonority("note-c4-e4-g4");
    Framework *framework;
    assert(framework_create(make_id("framework-common-practice-tonal-harmony"),
                             "common-practice-tonal-harmony", &framework) == FRAMEWORK_OK);

    UncertaintyCandidate candidates[2] = {
        {"C major triad", 3, 4},
        {"A minor 6th (1st inv.)", 1, 4},
    };

    /* Behavioral contract: a NULL sonority is rejected. */
    ChordIdentificationInput no_sonority_input = {NULL, framework, candidates, 2};
    AnalysisResult *no_sonority_result;
    assert(chord_identification_analyze(&no_sonority_input, NULL, NULL, &no_sonority_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: a NULL framework is rejected. */
    ChordIdentificationInput no_framework_input = {sonority, NULL, candidates, 2};
    AnalysisResult *no_framework_result;
    assert(chord_identification_analyze(&no_framework_input, NULL, NULL, &no_framework_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: an empty candidate set is rejected. */
    ChordIdentificationInput no_candidates_input = {sonority, framework, NULL, 0};
    AnalysisResult *no_candidates_result;
    assert(chord_identification_analyze(&no_candidates_input, NULL, NULL, &no_candidates_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Analysis-regression: "C major triad" (3/4) beats "A minor 6th (1st inv.)" (1/4). */
    ChordIdentificationInput input = {sonority, framework, candidates, 2};

    AnalysisResult *result;
    assert(CHORD_IDENTIFICATION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "note-c4-e4-g4") == 0);
    assert(interpretation_status(interpretation) == INTERPRETATION_STATUS_MACHINE_GENERATED);

    size_t claim_size;
    const char *claim_content = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(claim_size == strlen("C major triad") + 1);
    assert(strcmp(claim_content, "C major triad") == 0);

    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_CANDIDATE_SET);
    assert(uncertainty_candidate_count(uncertainty) == 2);
    assert(strcmp(uncertainty_candidate_label_at(uncertainty, 0), "C major triad") == 0);
    assert(uncertainty_candidate_weight_numerator_at(uncertainty, 0) == 3);
    assert(uncertainty_candidate_weight_denominator_at(uncertainty, 0) == 4);
    assert(strcmp(uncertainty_candidate_label_at(uncertainty, 1), "A minor 6th (1st inv.)") == 0);

    const Evidence *evidence = analysis_result_evidence_at(result, 0);
    assert(strcmp(evidence_method(evidence), "framework-relative chord matching") == 0);
    assert(strcmp(evidence_source(evidence), "common-practice-tonal-harmony") == 0);
    assert(evidence_strength_numerator(evidence) == 3);
    assert(evidence_strength_denominator(evidence) == 4);

    analysis_result_destroy(result);
    framework_destroy(framework);
    sonority_destroy(sonority);

    printf("test_chord_identification: ok\n");
    return 0;
}