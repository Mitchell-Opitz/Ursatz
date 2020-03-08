#include <assert.h>
#include <stdio.h>

#include "analysis/sentence_continuation_matching.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "time/duration.h"

/*
 * Analysis-regression tests: a candidate whose Contour direction sequence and
 * RhythmicPattern duration sequence are a strict prefix of the statement's
 * (shorter, min 2 events) qualifies as a continuation, whether or not it is
 * transposed (Contour is direction-only, transposition-invariant by
 * construction). A candidate that diverges in direction, diverges in
 * duration, is too short (< 2 events), or is not strictly shorter than the
 * statement does not qualify.
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

static Contour *make_contour_3(const char *p0, const char *p1, const char *p2) {
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

static Contour *make_contour_2_down(const char *p0, const char *p1) {
    PitchIdentity *pitch_ids[2] = {make_pitch(p0), make_pitch(p1)};
    ContourDirection directions[1] = {CONTOUR_DIRECTION_DOWN};
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

static RhythmicPattern *make_rhythm_mismatched_2(void) {
    Duration *quarter;
    assert(duration_create(1, 4, &quarter) == DURATION_OK);
    Duration *half;
    assert(duration_create(1, 2, &half) == DURATION_OK);
    Duration *durations[2] = {quarter, half};
    RhythmicPattern *pattern;
    assert(rhythmic_pattern_create(durations, 2, &pattern) == RHYTHMIC_PATTERN_OK);
    return pattern;
}

static MotifWindow make_window(Contour *contour, RhythmicPattern *rhythm, EntityID *const *event_ids,
                                size_t event_id_count) {
    MotifWindow window;
    window.contour = contour;
    window.rhythm = rhythm;
    window.event_ids = (const EntityID *const *)event_ids;
    window.event_id_count = event_id_count;
    return window;
}

int main(void) {
    EntityID *statement_events[3] = {make_id("note-s-0"), make_id("note-s-1"), make_id("note-s-2")};
    Contour *statement_contour = make_contour_3("c\0", "e\0", "d\0");
    RhythmicPattern *statement_rhythm = make_rhythm(3);
    MotifWindow statement = make_window(statement_contour, statement_rhythm, statement_events, 3);

    /* Shorter prefix, same directions/durations, transposed pitch tokens -- qualifies. */
    EntityID *transposed_events[2] = {make_id("note-t-0"), make_id("note-t-1")};
    Contour *transposed_contour = make_contour_2_up("g\0", "b\0");
    RhythmicPattern *transposed_rhythm = make_rhythm(2);
    MotifWindow transposed_candidate = make_window(transposed_contour, transposed_rhythm, transposed_events, 2);
    assert(sentence_continuation_matching_is_continuation(&statement, &transposed_candidate));

    /* Direction diverges from the statement's prefix -- does not qualify. */
    EntityID *wrong_direction_events[2] = {make_id("note-w-0"), make_id("note-w-1")};
    Contour *wrong_direction_contour = make_contour_2_down("c\0", "a\0");
    RhythmicPattern *wrong_direction_rhythm = make_rhythm(2);
    MotifWindow wrong_direction_candidate =
        make_window(wrong_direction_contour, wrong_direction_rhythm, wrong_direction_events, 2);
    assert(!sentence_continuation_matching_is_continuation(&statement, &wrong_direction_candidate));

    /* Same direction prefix, but rhythm diverges -- does not qualify. */
    EntityID *wrong_rhythm_events[2] = {make_id("note-r-0"), make_id("note-r-1")};
    Contour *wrong_rhythm_contour = make_contour_2_up("f\0", "a\0");
    RhythmicPattern *wrong_rhythm_rhythm = make_rhythm_mismatched_2();
    MotifWindow wrong_rhythm_candidate = make_window(wrong_rhythm_contour, wrong_rhythm_rhythm, wrong_rhythm_events, 2);
    assert(!sentence_continuation_matching_is_continuation(&statement, &wrong_rhythm_candidate));

    /* Single note (1 event, 0 directions) is below the minimum-length floor -- does not qualify. */
    EntityID *lone_event[1] = {make_id("note-lone-0")};
    PitchIdentity *lone_pitch_ids[2] = {make_pitch("c\0"), make_pitch("e\0")};
    ContourDirection lone_directions[1] = {CONTOUR_DIRECTION_UP};
    Contour *lone_contour;
    assert(contour_create(lone_pitch_ids, 2, lone_directions, 1, &lone_contour) == CONTOUR_OK);
    Duration *lone_duration;
    assert(duration_create(1, 4, &lone_duration) == DURATION_OK);
    Duration *lone_durations[1] = {lone_duration};
    RhythmicPattern *lone_rhythm;
    assert(rhythmic_pattern_create(lone_durations, 1, &lone_rhythm) == RHYTHMIC_PATTERN_OK);
    MotifWindow lone_candidate = make_window(lone_contour, lone_rhythm, lone_event, 1);
    assert(!sentence_continuation_matching_is_continuation(&statement, &lone_candidate));

    /* A candidate the same length as the statement is not "shortened" -- does not qualify. */
    EntityID *same_length_events[3] = {make_id("note-l-0"), make_id("note-l-1"), make_id("note-l-2")};
    Contour *same_length_contour = make_contour_3("g\0", "b\0", "a\0");
    RhythmicPattern *same_length_rhythm = make_rhythm(3);
    MotifWindow same_length_candidate = make_window(same_length_contour, same_length_rhythm, same_length_events, 3);
    assert(!sentence_continuation_matching_is_continuation(&statement, &same_length_candidate));

    /* NULL candidate/statement is rejected, never guessed at. */
    assert(!sentence_continuation_matching_is_continuation(&statement, NULL));
    assert(!sentence_continuation_matching_is_continuation(NULL, &transposed_candidate));

    for (size_t i = 0; i < 3; i++) {
        entity_id_destroy(statement_events[i]);
    }
    for (size_t i = 0; i < 2; i++) {
        entity_id_destroy(transposed_events[i]);
        entity_id_destroy(wrong_direction_events[i]);
        entity_id_destroy(wrong_rhythm_events[i]);
    }
    for (size_t i = 0; i < 3; i++) {
        entity_id_destroy(same_length_events[i]);
    }
    entity_id_destroy(lone_event[0]);

    contour_destroy(statement_contour);
    contour_destroy(transposed_contour);
    contour_destroy(wrong_direction_contour);
    contour_destroy(wrong_rhythm_contour);
    contour_destroy(lone_contour);
    contour_destroy(same_length_contour);
    rhythmic_pattern_destroy(statement_rhythm);
    rhythmic_pattern_destroy(transposed_rhythm);
    rhythmic_pattern_destroy(wrong_direction_rhythm);
    rhythmic_pattern_destroy(wrong_rhythm_rhythm);
    rhythmic_pattern_destroy(lone_rhythm);
    rhythmic_pattern_destroy(same_length_rhythm);

    printf("test_sentence_continuation_matching: ok\n");
    return 0;
}
