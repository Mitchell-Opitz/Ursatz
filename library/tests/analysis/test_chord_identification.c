#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/chord_identification.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "interpretation/evidence.h"
#include "interpretation/interpretation.h"
#include "semantics/sonority.h"
#include "theory/framework.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/*
 * Behavioral-contract test: ChordIdentification conforms to the
 * Analyzer interface (analysis/analyzer.h) -- NULL input, a NULL
 * sonority, a NULL framework, and an empty scale-degree set are all
 * rejected without touching *out.
 *
 * Identification tests: each of common-practice-minimal-v1's seven
 * diatonic triads, given real scale degrees, resolves to a real Chord
 * (via frameworks/common-practice-minimal/triad_identification.h)
 * carried as this analyzer's Interpretation. A degree set matching none
 * of the seven reports ANALYZER_ERR_ANALYSIS_FAILED rather than a
 * forced guess.
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

static void check_triad(Framework *framework, const int *degrees, size_t degree_count, const char *expected_label,
                         const char *sonority_event_id) {
    Sonority *sonority = make_sonority(sonority_event_id);
    ChordIdentificationInput input = {sonority, framework, degrees, degree_count, NULL, 0};

    AnalysisResult *result;
    assert(CHORD_IDENTIFICATION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), sonority_event_id) == 0);
    assert(interpretation_framework_reference(interpretation) != NULL);

    size_t claim_size;
    const char *claim_content = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(strcmp(claim_content, expected_label) == 0);

    analysis_result_destroy(result);
    sonority_destroy(sonority);
}

static void check_seventh(Framework *framework, const int *intervals, size_t interval_count,
                           const char *expected_label, const char *sonority_event_id) {
    Sonority *sonority = make_sonority(sonority_event_id);
    ChordIdentificationInput input = {sonority, framework, NULL, 0, intervals, interval_count};

    AnalysisResult *result;
    assert(CHORD_IDENTIFICATION_ANALYZER.analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 1);
    assert(analysis_result_evidence_count(result) == 1);

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), sonority_event_id) == 0);
    assert(interpretation_framework_reference(interpretation) != NULL);

    size_t claim_size;
    const char *claim_content = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
    assert(strcmp(claim_content, expected_label) == 0);

    analysis_result_destroy(result);
    sonority_destroy(sonority);
}

int main(void) {
    /* Behavioral contract: NULL input is rejected without touching *out. */
    AnalysisResult *untouched = (AnalysisResult *)0x1;
    assert(CHORD_IDENTIFICATION_ANALYZER.analyze(NULL, NULL, NULL, &untouched) == ANALYZER_ERR_NULL_INPUT);
    assert(untouched == (AnalysisResult *)0x1);
    assert(strcmp(CHORD_IDENTIFICATION_ANALYZER.name, "ChordIdentification") == 0);

    Framework *framework;
    assert(framework_create(make_id("framework-common-practice-minimal"), "common-practice-minimal-v1",
                             &framework) == FRAMEWORK_OK);

    Sonority *sonority = make_sonority("note-c4-e4-g4");
    int degrees[] = {1, 3, 5};

    /* Behavioral contract: a NULL sonority is rejected. */
    ChordIdentificationInput no_sonority_input = {NULL, framework, degrees, 3};
    AnalysisResult *no_sonority_result;
    assert(chord_identification_analyze(&no_sonority_input, NULL, NULL, &no_sonority_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: a NULL framework is rejected. */
    ChordIdentificationInput no_framework_input = {sonority, NULL, degrees, 3};
    AnalysisResult *no_framework_result;
    assert(chord_identification_analyze(&no_framework_input, NULL, NULL, &no_framework_result) ==
           ANALYZER_ERR_NULL_INPUT);

    /* Behavioral contract: an empty scale-degree set is rejected. */
    ChordIdentificationInput no_degrees_input = {sonority, framework, NULL, 0};
    AnalysisResult *no_degrees_result;
    assert(chord_identification_analyze(&no_degrees_input, NULL, NULL, &no_degrees_result) == ANALYZER_ERR_NULL_INPUT);

    sonority_destroy(sonority);

    /* Identification: each of the seven diatonic triads. */
    int degrees_i[] = {1, 3, 5};
    check_triad(framework, degrees_i, 3, "I", "sonority-I");
    int degrees_ii[] = {2, 4, 6};
    check_triad(framework, degrees_ii, 3, "ii", "sonority-ii");
    int degrees_iii[] = {3, 5, 7};
    check_triad(framework, degrees_iii, 3, "iii", "sonority-iii");
    int degrees_iv[] = {4, 6, 1};
    check_triad(framework, degrees_iv, 3, "IV", "sonority-IV");
    int degrees_v[] = {5, 7, 2};
    check_triad(framework, degrees_v, 3, "V", "sonority-V");
    int degrees_vi[] = {6, 1, 3};
    check_triad(framework, degrees_vi, 3, "vi", "sonority-vi");
    int degrees_vii[] = {7, 2, 4};
    check_triad(framework, degrees_vii, 3, "vii\xC2\xB0", "sonority-vii-dim");

    /* No match: not one of the seven diatonic triads -- reported as a
       failed analysis, never a forced guess. */
    Sonority *no_match_sonority = make_sonority("sonority-no-match");
    int no_match_degrees[] = {1, 2, 4};
    ChordIdentificationInput no_match_input = {no_match_sonority, framework, no_match_degrees, 3, NULL, 0};
    AnalysisResult *no_match_result;
    assert(chord_identification_analyze(&no_match_input, NULL, NULL, &no_match_result) ==
           ANALYZER_ERR_ANALYSIS_FAILED);
    sonority_destroy(no_match_sonority);

    /* Identification: each of the four diatonic seventh-chord qualities. */
    int dominant7[] = {0, 4, 7, 10};
    check_seventh(framework, dominant7, 4, "dominant7", "sonority-dominant7");
    int major7[] = {0, 4, 7, 11};
    check_seventh(framework, major7, 4, "major7", "sonority-major7");
    int minor7[] = {0, 3, 7, 10};
    check_seventh(framework, minor7, 4, "minor7", "sonority-minor7");
    int half_diminished7[] = {0, 3, 6, 10};
    check_seventh(framework, half_diminished7, 4, "half-diminished7", "sonority-half-diminished7");

    /* No match: fully-diminished7 is not one of the four qualities this
       framework knows -- reported as a failed analysis, never a forced
       guess. */
    Sonority *no_seventh_match_sonority = make_sonority("sonority-no-seventh-match");
    int no_seventh_match_intervals[] = {0, 3, 6, 9};
    ChordIdentificationInput no_seventh_match_input = {
        no_seventh_match_sonority, framework, NULL, 0, no_seventh_match_intervals, 4};
    AnalysisResult *no_seventh_match_result;
    assert(chord_identification_analyze(&no_seventh_match_input, NULL, NULL, &no_seventh_match_result) ==
           ANALYZER_ERR_ANALYSIS_FAILED);
    sonority_destroy(no_seventh_match_sonority);

    framework_destroy(framework);

    printf("test_chord_identification: ok\n");
    return 0;
}
