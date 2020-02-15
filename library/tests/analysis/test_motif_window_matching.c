#include <assert.h>
#include <stdio.h>

#include "analysis/motif_detection.h"
#include "analysis/motif_window_matching.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "structure/motif.h"
#include "time/duration.h"

/*
 * Analysis-regression tests: exact repetition and transposition (same
 * Contour directions, different PitchIdentity tokens) both match;
 * rhythm-mismatch and contour-mismatch windows do not; a window with no
 * partner anywhere in the input produces no occurrence. The matched
 * occurrences produced here also feed straight into
 * motif_detection_analyze, pinning the whole pipeline end to end.
 */

static EntityID *make_id(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static Motif *make_motif_stub(void) {
    EntityID *event_ids[1] = {make_id("note-motif-def-1")};
    Motif *motif;
    assert(motif_create(make_id("motif-window-matching-stub"), event_ids, 1, &motif) == MOTIF_OK);
    return motif;
}

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *identity;
    assert(pitch_identity_create(token, 2, &identity) == PITCH_IDENTITY_OK);
    return identity;
}

static Contour *make_contour_up_down(const char *p0, const char *p1, const char *p2) {
    PitchIdentity *pitch_ids[3] = {make_pitch(p0), make_pitch(p1), make_pitch(p2)};
    ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_DOWN};
    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);
    return contour;
}

static Contour *make_contour_up_same(const char *p0, const char *p1, const char *p2) {
    PitchIdentity *pitch_ids[3] = {make_pitch(p0), make_pitch(p1), make_pitch(p2)};
    ContourDirection directions[2] = {CONTOUR_DIRECTION_UP, CONTOUR_DIRECTION_SAME};
    Contour *contour;
    assert(contour_create(pitch_ids, 3, directions, 2, &contour) == CONTOUR_OK);
    return contour;
}

static RhythmicPattern *make_rhythm_quarter_quarter(void) {
    Duration *quarter_a;
    assert(duration_create(1, 4, &quarter_a) == DURATION_OK);
    Duration *quarter_b;
    assert(duration_create(1, 4, &quarter_b) == DURATION_OK);
    Duration *durations[2] = {quarter_a, quarter_b};
    RhythmicPattern *pattern;
    assert(rhythmic_pattern_create(durations, 2, &pattern) == RHYTHMIC_PATTERN_OK);
    return pattern;
}

static RhythmicPattern *make_rhythm_quarter_half(void) {
    Duration *quarter;
    assert(duration_create(1, 4, &quarter) == DURATION_OK);
    Duration *half;
    assert(duration_create(1, 2, &half) == DURATION_OK);
    Duration *durations[2] = {quarter, half};
    RhythmicPattern *pattern;
    assert(rhythmic_pattern_create(durations, 2, &pattern) == RHYTHMIC_PATTERN_OK);
    return pattern;
}

int main(void) {
    /* Window 0 and window 1: exact repetition (same contour shape, same pitch tokens, same rhythm). */
    Contour *contour_0 = make_contour_up_down("c\0", "e\0", "d\0");
    Contour *contour_1 = make_contour_up_down("c\0", "e\0", "d\0");
    RhythmicPattern *rhythm_0 = make_rhythm_quarter_quarter();
    RhythmicPattern *rhythm_1 = make_rhythm_quarter_quarter();

    /* Window 2: transposition of window 0/1 -- same directions, different pitch tokens, same rhythm. */
    Contour *contour_2 = make_contour_up_down("g\0", "b\0", "a\0");
    RhythmicPattern *rhythm_2 = make_rhythm_quarter_quarter();

    /* Window 3: same rhythm as 0/1/2 but a different contour shape -- must not match. */
    Contour *contour_3 = make_contour_up_same("c\0", "e\0", "e\0");
    RhythmicPattern *rhythm_3 = make_rhythm_quarter_quarter();

    /* Window 4: same contour shape as 0/1/2 but a different rhythm -- must not match. */
    Contour *contour_4 = make_contour_up_down("f\0", "a\0", "g\0");
    RhythmicPattern *rhythm_4 = make_rhythm_quarter_half();

    EntityID *event_ids_0[2] = {make_id("note-0-a"), make_id("note-0-b")};
    EntityID *event_ids_1[2] = {make_id("note-1-a"), make_id("note-1-b")};
    EntityID *event_ids_2[2] = {make_id("note-2-a"), make_id("note-2-b")};
    EntityID *event_ids_3[2] = {make_id("note-3-a"), make_id("note-3-b")};
    EntityID *event_ids_4[2] = {make_id("note-4-a"), make_id("note-4-b")};

    MotifWindow windows[5] = {
        {contour_0, rhythm_0, (const EntityID *const *)event_ids_0, 2},
        {contour_1, rhythm_1, (const EntityID *const *)event_ids_1, 2},
        {contour_2, rhythm_2, (const EntityID *const *)event_ids_2, 2},
        {contour_3, rhythm_3, (const EntityID *const *)event_ids_3, 2},
        {contour_4, rhythm_4, (const EntityID *const *)event_ids_4, 2},
    };

    MotifDetectionOccurrence *occurrences;
    size_t occurrence_count;
    assert(motif_window_matching_find(windows, 5, &occurrences, &occurrence_count) == MOTIF_WINDOW_MATCHING_OK);

    /* Only windows 0, 1, and 2 (exact repetition + transposition) matched each other. */
    assert(occurrence_count == 3);
    for (size_t i = 0; i < occurrence_count; i++) {
        assert(occurrences[i].confidence_numerator == 1);
        assert(occurrences[i].confidence_denominator == 1);
        assert(occurrences[i].matched_event_id_count == 2);
    }

    /* Feed straight into MotifDetection: the matched windows package into one Interpretation each. */
    Motif *motif = make_motif_stub();
    MotifDetectionInput input = {motif, occurrences, occurrence_count};
    AnalysisResult *result;
    assert(motif_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK);
    assert(analysis_result_interpretation_count(result) == 3);
    analysis_result_destroy(result);

    motif_window_matching_destroy_occurrences(occurrences);

    /* A window with no partner anywhere in the input yields no occurrence. */
    MotifWindow lone_window[1] = {{contour_4, rhythm_4, (const EntityID *const *)event_ids_4, 2}};
    MotifDetectionOccurrence *lone_occurrences;
    size_t lone_count;
    assert(motif_window_matching_find(lone_window, 1, &lone_occurrences, &lone_count) == MOTIF_WINDOW_MATCHING_OK);
    assert(lone_count == 0);
    assert(lone_occurrences == NULL);

    /* Empty input is valid and yields no occurrences. */
    MotifDetectionOccurrence *empty_occurrences;
    size_t empty_count;
    assert(motif_window_matching_find(NULL, 0, &empty_occurrences, &empty_count) == MOTIF_WINDOW_MATCHING_OK);
    assert(empty_count == 0);
    assert(empty_occurrences == NULL);

    motif_destroy(motif);
    for (size_t i = 0; i < 2; i++) {
        entity_id_destroy(event_ids_0[i]);
        entity_id_destroy(event_ids_1[i]);
        entity_id_destroy(event_ids_2[i]);
        entity_id_destroy(event_ids_3[i]);
        entity_id_destroy(event_ids_4[i]);
    }
    contour_destroy(contour_0);
    contour_destroy(contour_1);
    contour_destroy(contour_2);
    contour_destroy(contour_3);
    contour_destroy(contour_4);
    rhythmic_pattern_destroy(rhythm_0);
    rhythmic_pattern_destroy(rhythm_1);
    rhythmic_pattern_destroy(rhythm_2);
    rhythmic_pattern_destroy(rhythm_3);
    rhythmic_pattern_destroy(rhythm_4);

    printf("test_motif_window_matching: ok\n");
    return 0;
}
