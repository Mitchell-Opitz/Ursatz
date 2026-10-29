#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "analysis/motif_window_matching.h"
#include "analysis/phrase_boundary_detection.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "interpretation/interpretation.h"
#include "pitch/pitch_identity.h"
#include "semantics/cadence_type.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "ursatz_chain/form_bridge.h"

/*
 * The chain's two form bridges: boundary points with cadences, discontinuities and neither, a run longer than max_length, count 0
 * and NULL arguments; windows that match and do not match. Every answer is also compared with calling the kernel detectors
 * directly.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static NoteEvent *make_note_event(const char *id_value, const char *pitch_token) {
    MusicalTime *position;
    assert(musical_time_create(0, 4, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration;
    assert(duration_create(1, 4, &duration) == DURATION_OK);
    PitchIdentity *pitch;
    assert(pitch_identity_create(pitch_token, strlen(pitch_token) + 1, &pitch) == PITCH_IDENTITY_OK);
    ProvenanceReference *provenance;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent *note_event;
    assert(note_event_create(make_id(id_value), anchor, duration, pitch, NULL, NULL, provenance, &note_event) == NOTE_EVENT_OK);
    return note_event;
}

/* The kernel called directly: is_boundary per point, and the signal (cadence or not) per boundary point in order. */
static void kernel_direct(const StandardFormBoundaryPoint *points, size_t count, size_t max_length, bool *is_boundary,
                          bool *by_cadence) {
    PhraseBoundaryDetectionPoint kernel_points[16];
    assert(count <= 16);
    for (size_t i = 0; i < count; i++) {
        kernel_points[i].event = points[i].event;
        kernel_points[i].has_cadence = points[i].has_cadence;
        kernel_points[i].cadence_type = points[i].has_cadence ? cadence_type_label(points[i].cadence_type) : NULL;
        kernel_points[i].has_discontinuity = points[i].has_discontinuity;
    }
    PhraseBoundaryDetectionInput input = {kernel_points, count, max_length};
    AnalysisResult *boundary_result;
    AnalysisResult *signal_result;
    assert(phrase_boundary_detection_analyze(&input, NULL, NULL, &boundary_result) == ANALYZER_OK);
    assert(phrase_boundary_detection_signal_analyze(&input, NULL, NULL, &signal_result) == ANALYZER_OK);
    size_t signal_index = 0;
    for (size_t i = 0; i < count; i++) {
        size_t size;
        const PhraseSegmentationClaim *boundary = (const PhraseSegmentationClaim *)value_content(
            interpretation_claim(analysis_result_interpretation_at(boundary_result, i)), &size);
        is_boundary[i] = boundary->is_boundary;
        by_cadence[i] = false;
        if (boundary->is_boundary) {
            const PhraseBoundaryDetectionSignalClaim *signal = (const PhraseBoundaryDetectionSignalClaim *)value_content(
                interpretation_claim(analysis_result_interpretation_at(signal_result, signal_index++)), &size);
            by_cadence[i] = signal->signal == PHRASE_BOUNDARY_SIGNAL_CADENCE;
        }
    }
    analysis_result_destroy(boundary_result);
    analysis_result_destroy(signal_result);
}

static void check_against_kernel(const StandardFormBoundaryPoint *points, size_t count, size_t max_length,
                                 StandardFormBoundary *out) {
    bool is_boundary[16];
    bool by_cadence[16];
    kernel_direct(points, count, max_length, is_boundary, by_cadence);
    for (size_t i = 0; i < count; i++) {
        assert(out[i].is_boundary == is_boundary[i]);
        assert(out[i].by_cadence == by_cadence[i]);
    }
}

static Contour *make_contour(const char *p0, const char *p1, const char *p2, ContourDirection second) {
    const char *tokens[3] = {p0, p1, p2};
    PitchIdentity *pitch_ids[3];
    for (int i = 0; i < 3; i++) {
        assert(pitch_identity_create(tokens[i], 2, &pitch_ids[i]) == PITCH_IDENTITY_OK);
    }
    ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, second};
    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);
    return contour;
}

static RhythmicPattern *make_rhythm(long second_numerator, unsigned long second_denominator) {
    Duration *first;
    assert(duration_create(1, 4, &first) == DURATION_OK);
    Duration *second;
    assert(duration_create(second_numerator, second_denominator, &second) == DURATION_OK);
    Duration *durations[2] = {first, second};
    RhythmicPattern *pattern;
    assert(rhythmic_pattern_create(durations, 2, &pattern) == RHYTHMIC_PATTERN_OK);
    return pattern;
}

static void test_boundaries(void) {
    NoteEvent *events[6];
    const char *pitches[6] = {"C4", "D4", "E4", "F4", "G4", "A4"};
    for (int i = 0; i < 6; i++) {
        char id[16];
        snprintf(id, sizeof(id), "note-%d", i);
        events[i] = make_note_event(id, pitches[i]);
    }

    /* Cadence, discontinuity and neither (max_length 3): the kernel test's sequence. */
    StandardFormBoundaryPoint mixed[5] = {
        {events[0], false, CADENCE_TYPE_UNKNOWN, false},
        {events[1], false, CADENCE_TYPE_UNKNOWN, false},
        {events[2], false, CADENCE_TYPE_UNKNOWN, true},
        {events[3], true, CADENCE_TYPE_UNKNOWN, false},
        {events[4], false, CADENCE_TYPE_UNKNOWN, false},
    };
    StandardFormBoundary out[6];
    memset(out, 0xff, sizeof(out));
    assert(chain_form_boundary_detect(mixed, 5, 3, out, NULL) == STANDARD_FORM_ANALYSIS_OK);
    static const bool expected_boundary[5] = {false, false, true, true, false};
    static const bool expected_cadence[5] = {false, false, false, true, false};
    for (size_t i = 0; i < 5; i++) {
        assert(out[i].is_boundary == expected_boundary[i]);
        assert(out[i].by_cadence == expected_cadence[i]);
    }
    check_against_kernel(mixed, 5, 3, out);

    /* A classified cadence is a cadence boundary too. */
    StandardFormBoundaryPoint classified[2] = {
        {events[0], false, CADENCE_TYPE_UNKNOWN, false},
        {events[1], true, CADENCE_TYPE_PERFECT_AUTHENTIC, false},
    };
    assert(chain_form_boundary_detect(classified, 2, 3, out, NULL) == STANDARD_FORM_ANALYSIS_OK);
    assert(!out[0].is_boundary && out[1].is_boundary && out[1].by_cadence);
    check_against_kernel(classified, 2, 3, out);

    /* A run longer than max_length: a discontinuity inside max_length is not a boundary, one after it is. */
    StandardFormBoundaryPoint run[6] = {
        {events[0], false, CADENCE_TYPE_UNKNOWN, false}, {events[1], false, CADENCE_TYPE_UNKNOWN, true},
        {events[2], false, CADENCE_TYPE_UNKNOWN, false}, {events[3], false, CADENCE_TYPE_UNKNOWN, false},
        {events[4], false, CADENCE_TYPE_UNKNOWN, true},  {events[5], false, CADENCE_TYPE_UNKNOWN, false},
    };
    assert(chain_form_boundary_detect(run, 6, 3, out, NULL) == STANDARD_FORM_ANALYSIS_OK);
    assert(!out[1].is_boundary);
    assert(out[4].is_boundary && !out[4].by_cadence);
    check_against_kernel(run, 6, 3, out);

    /* Neither signal anywhere: no boundary. */
    StandardFormBoundaryPoint plain[3] = {
        {events[0], false, CADENCE_TYPE_UNKNOWN, false},
        {events[1], false, CADENCE_TYPE_UNKNOWN, false},
        {events[2], false, CADENCE_TYPE_UNKNOWN, false},
    };
    assert(chain_form_boundary_detect(plain, 3, 3, out, NULL) == STANDARD_FORM_ANALYSIS_OK);
    for (size_t i = 0; i < 3; i++) {
        assert(!out[i].is_boundary && !out[i].by_cadence);
    }
    check_against_kernel(plain, 3, 3, out);

    /* count 0 is valid and leaves nothing to answer. */
    assert(chain_form_boundary_detect(plain, 0, 3, out, NULL) == STANDARD_FORM_ANALYSIS_OK);

    /* NULL arguments. */
    assert(chain_form_boundary_detect(NULL, 3, 3, out, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(chain_form_boundary_detect(plain, 3, 3, NULL, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);

    for (int i = 0; i < 6; i++) {
        note_event_destroy(events[i]);
    }
}

static void test_windows(void) {
    /* Windows 0 and 1: the same shape (a repetition). Window 2: a transposition of it. Window 3: another contour. Window 4: another rhythm. */
    Contour *contours[5] = {make_contour("c\0", "e\0", "d\0", CONTOUR_DIRECTION_DOWN),
                            make_contour("c\0", "e\0", "d\0", CONTOUR_DIRECTION_DOWN),
                            make_contour("g\0", "b\0", "a\0", CONTOUR_DIRECTION_DOWN),
                            make_contour("c\0", "e\0", "e\0", CONTOUR_DIRECTION_SAME),
                            make_contour("f\0", "a\0", "g\0", CONTOUR_DIRECTION_DOWN)};
    RhythmicPattern *rhythms[5] = {make_rhythm(1, 4), make_rhythm(1, 4), make_rhythm(1, 4), make_rhythm(1, 4), make_rhythm(1, 2)};
    EntityID *ids[5][2];
    StandardFormMotifWindow windows[5];
    MotifWindow kernel_windows[5];
    for (int i = 0; i < 5; i++) {
        char first[16];
        char second[16];
        snprintf(first, sizeof(first), "note-%d-a", i);
        snprintf(second, sizeof(second), "note-%d-b", i);
        ids[i][0] = make_id(first);
        ids[i][1] = make_id(second);
        windows[i] = (StandardFormMotifWindow){contours[i], rhythms[i], (const EntityID *const *)ids[i], 2};
        kernel_windows[i] = (MotifWindow){contours[i], rhythms[i], (const EntityID *const *)ids[i], 2};
    }

    bool matched[5];
    memset(matched, 0xff, sizeof(matched));
    assert(chain_form_match_windows(windows, 5, matched, NULL) == STANDARD_FORM_ANALYSIS_OK);
    static const bool expected[5] = {true, true, true, false, false};
    for (int i = 0; i < 5; i++) {
        assert(matched[i] == expected[i]);
    }

    /* The kernel called directly names the same windows. */
    MotifDetectionOccurrence *found;
    size_t found_count;
    assert(motif_window_matching_find(kernel_windows, 5, &found, &found_count) == MOTIF_WINDOW_MATCHING_OK);
    for (int i = 0; i < 5; i++) {
        bool direct = false;
        for (size_t j = 0; j < found_count; j++) {
            if (found[j].matched_event_ids == windows[i].event_ids) {
                direct = true;
            }
        }
        assert(matched[i] == direct);
    }
    motif_window_matching_destroy_occurrences(found);

    /* A window with no partner does not match. */
    assert(chain_form_match_windows(&windows[3], 1, matched, NULL) == STANDARD_FORM_ANALYSIS_OK);
    assert(!matched[0]);

    /* count 0 is valid. */
    assert(chain_form_match_windows(windows, 0, matched, NULL) == STANDARD_FORM_ANALYSIS_OK);

    /* NULL arguments. */
    assert(chain_form_match_windows(NULL, 5, matched, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(chain_form_match_windows(windows, 5, NULL, NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);

    for (int i = 0; i < 5; i++) {
        entity_id_destroy(ids[i][0]);
        entity_id_destroy(ids[i][1]);
        contour_destroy(contours[i]);
        rhythmic_pattern_destroy(rhythms[i]);
    }
}

int main(void) {
    test_boundaries();
    test_windows();
    printf("test_form_bridge: ok\n");
    return 0;
}
