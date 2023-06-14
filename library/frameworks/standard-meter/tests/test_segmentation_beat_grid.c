#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "meter_test_util.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "segmentation_beat_grid.h"
#include "semantics/sonority_segmentation.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

/*
 * The standard-meter beat-grid provider for adaptive sonority segmentation: the beat starts of a
 * measure from the signature's simple/compound reading, including short measures and pickups, and the
 * provider handed to the kernel builder.
 */

typedef struct {
    unsigned top;
    int bottom_exponent;
    long start_n;
    unsigned long start_d;
} Change;

static MeterMap *build_map(const Change *changes, size_t count) {
    TimeSignature *signatures[4];
    MusicalTime *starts[4];
    MeterMapChangePoint points[4];
    for (size_t i = 0; i < count; i++) {
        signatures[i] = make_signature(changes[i].top, changes[i].bottom_exponent);
        assert(musical_time_create(changes[i].start_n, changes[i].start_d, &starts[i]) == MUSICAL_TIME_OK);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    EntityID *id = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    return map;
}

/* The beat starts of `measure` must be exactly the given fractions (num/den pairs), in order. */
static void check_beats(const MeterMap *map, int measure, size_t expected_count, const long expected[][2]) {
    MusicalTime **starts = NULL;
    size_t count = 0;
    assert(standard_meter_segmentation_beat_grid(map, measure, NULL, &starts, &count));
    assert(count == expected_count);
    for (size_t i = 0; i < count; i++) {
        MusicalTime *want = NULL;
        assert(musical_time_create(expected[i][0], (unsigned long)expected[i][1], &want) == MUSICAL_TIME_OK);
        assert(musical_time_equals(starts[i], want));
        musical_time_destroy(want);
        musical_time_destroy(starts[i]);
    }
    free(starts);
}

static void test_simple_meters(void) {
    Change four_four[] = {{4, 2, 0, 1}};
    MeterMap *map = build_map(four_four, 1);
    const long quarters[4][2] = {{0, 1}, {1, 4}, {1, 2}, {3, 4}};
    check_beats(map, 1, 4, quarters);
    const long second[4][2] = {{1, 1}, {5, 4}, {3, 2}, {7, 4}};
    check_beats(map, 2, 4, second);
    meter_map_destroy(map);

    Change three_four[] = {{3, 2, 0, 1}};
    map = build_map(three_four, 1);
    const long three[3][2] = {{0, 1}, {1, 4}, {1, 2}};
    check_beats(map, 1, 3, three);
    meter_map_destroy(map);

    Change two_four[] = {{2, 2, 0, 1}};
    map = build_map(two_four, 1);
    const long two[2][2] = {{0, 1}, {1, 4}};
    check_beats(map, 1, 2, two);
    meter_map_destroy(map);
}

static void test_compound_meters(void) {
    Change six_eight[] = {{6, 3, 0, 1}};
    MeterMap *map = build_map(six_eight, 1);
    const long two_beats[2][2] = {{0, 1}, {3, 8}};
    check_beats(map, 1, 2, two_beats);
    const long second[2][2] = {{3, 4}, {9, 8}};
    check_beats(map, 2, 2, second);
    meter_map_destroy(map);

    Change nine_eight[] = {{9, 3, 0, 1}};
    map = build_map(nine_eight, 1);
    const long three_beats[3][2] = {{0, 1}, {3, 8}, {3, 4}};
    check_beats(map, 1, 3, three_beats);
    meter_map_destroy(map);

    Change twelve_eight[] = {{12, 3, 0, 1}};
    map = build_map(twelve_eight, 1);
    const long four_beats[4][2] = {{0, 1}, {3, 8}, {3, 4}, {9, 8}};
    check_beats(map, 1, 4, four_beats);
    const long next[4][2] = {{3, 2}, {15, 8}, {9, 4}, {21, 8}};
    check_beats(map, 2, 4, next);
    meter_map_destroy(map);
}

static void test_irregular_meters_are_bottom_unit_pulses(void) {
    Change five_eight[] = {{5, 3, 0, 1}};
    MeterMap *map = build_map(five_eight, 1);
    const long pulses[5][2] = {{0, 1}, {1, 8}, {1, 4}, {3, 8}, {1, 2}};
    check_beats(map, 1, 5, pulses);
    meter_map_destroy(map);
}

static void test_a_short_measure_ends_at_the_barline(void) {
    /* 4/4 cut at 5/2 by 3/4: measure 3 is [2, 5/2), two beats; measure 4 then starts a fresh grid. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 5, 2}};
    MeterMap *map = build_map(changes, 2);
    const long short_measure[2][2] = {{2, 1}, {9, 4}};
    check_beats(map, 3, 2, short_measure);
    const long after[3][2] = {{5, 2}, {11, 4}, {3, 1}};
    check_beats(map, 4, 3, after);
    meter_map_destroy(map);
}

static void test_a_pickup_measure(void) {
    /* A quarter-note pickup in 4/4 (4/4, then 3/4 from 1/4): the last beat of an imagined measure. */
    Change quarter[] = {{4, 2, 0, 1}, {3, 2, 1, 4}};
    MeterMap *map = build_map(quarter, 2);
    const long one_beat[1][2] = {{0, 1}};
    check_beats(map, 0, 1, one_beat);
    const long first_full[3][2] = {{1, 4}, {1, 2}, {3, 4}};
    check_beats(map, 1, 3, first_full);
    meter_map_destroy(map);

    /* An eighth-note pickup in 4/4 starts partway through beat 4: one partial beat. */
    Change eighth[] = {{4, 2, 0, 1}, {3, 2, 1, 8}};
    map = build_map(eighth, 2);
    check_beats(map, 0, 1, one_beat);
    meter_map_destroy(map);

    /* A 1/8 region before 12/8 is read in 12/8: the pickup eighth is within the last beat. */
    Change leading[] = {{1, 3, 0, 1}, {12, 3, 1, 8}};
    map = build_map(leading, 2);
    check_beats(map, 0, 1, one_beat);
    const long first_twelve[4][2] = {{1, 8}, {1, 2}, {7, 8}, {5, 4}};
    check_beats(map, 1, 4, first_twelve);
    meter_map_destroy(map);

    /* A five-eighth pickup in 6/8 (6/8, then 3/4 from 5/8) spans a beat line at 3/8 of the full
       measure: the pickup begins 1/8 into the first beat, so its second beat starts at 1/4. */
    Change spanning[] = {{6, 3, 0, 1}, {3, 2, 5, 8}};
    map = build_map(spanning, 2);
    const long two_beats[2][2] = {{0, 1}, {1, 4}};
    check_beats(map, 0, 2, two_beats);
    meter_map_destroy(map);
}

static void test_declines_bad_input(void) {
    Change changes[] = {{4, 2, 0, 1}};
    MeterMap *map = build_map(changes, 1);
    MusicalTime **starts = NULL;
    size_t count = 7;
    assert(!standard_meter_segmentation_beat_grid(NULL, 1, NULL, &starts, &count));
    assert(!standard_meter_segmentation_beat_grid(map, 1, NULL, NULL, &count));
    assert(!standard_meter_segmentation_beat_grid(map, 0, NULL, &starts, &count)); /* no pickup: no measure 0 */
    assert(starts == NULL && count == 7);
    meter_map_destroy(map);
}

/* --- the provider through the kernel builder --- */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static NoteEvent *make_note(const char *id, long on_n, unsigned long on_d, long dur_n, unsigned long dur_d,
                            PitchLetter letter) {
    MusicalTime *position = NULL;
    assert(musical_time_create(on_n, on_d, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = make_duration(dur_n, dur_d);
    PitchIdentity *pitch = NULL;
    assert(pitch_identity_create("pitch", 6, &pitch) == PITCH_IDENTITY_OK);
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, 0, 4, &spelling) == PITCH_SPELLING_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent *note = NULL;
    assert(note_event_create(make_id(id), anchor, duration, pitch, spelling, NULL, provenance, &note) ==
           NOTE_EVENT_OK);
    return note;
}

static void assert_window(const AdaptiveSonority *s, long sn, unsigned long sd, long en, unsigned long ed) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    assert(musical_time_create(sn, sd, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create(en, ed, &end) == MUSICAL_TIME_OK);
    const TimeSpan *window = sonority_time_window(s->sonority);
    assert(musical_time_equals(time_span_start(window), start));
    assert(musical_time_equals(time_span_end(window), end));
    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void test_the_builder_cuts_6_8_and_12_8_on_dotted_quarter_beats(void) {
    /* A different note on each dotted-quarter beat of 6/8: the segments fall at 3/8. */
    Change six_eight[] = {{6, 3, 0, 1}};
    MeterMap *map = build_map(six_eight, 1);
    NoteEvent *first = make_note("c", 0, 1, 3, 8, PITCH_LETTER_C);
    NoteEvent *second = make_note("d", 3, 8, 3, 8, PITCH_LETTER_D);
    NoteEvent *two[2] = {first, second};
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(two, 2, map, standard_meter_segmentation_beat_grid, NULL, NULL,
                                           &out) == AD_SEGMENTATION_OK);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 3, 8);
    assert_window(&out.items[1], 3, 8, 3, 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    note_event_destroy(first);
    note_event_destroy(second);

    /* Four notes on the four dotted-quarter beats of 12/8. */
    Change twelve_eight[] = {{12, 3, 0, 1}};
    map = build_map(twelve_eight, 1);
    NoteEvent *beats[4] = {make_note("b1", 0, 1, 3, 8, PITCH_LETTER_C), make_note("b2", 3, 8, 3, 8, PITCH_LETTER_D),
                           make_note("b3", 3, 4, 3, 8, PITCH_LETTER_E), make_note("b4", 9, 8, 3, 8, PITCH_LETTER_F)};
    assert(sonority_builder_build_adaptive(beats, 4, map, standard_meter_segmentation_beat_grid, NULL, NULL,
                                           &out) == AD_SEGMENTATION_OK);
    assert(out.count == 4);
    assert_window(&out.items[1], 3, 8, 3, 4);
    assert_window(&out.items[3], 9, 8, 3, 2);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    for (size_t i = 0; i < 4; i++) {
        note_event_destroy(beats[i]);
    }

    /* The same note held over the whole of a 12/8 measure is one segment: the beat lines are cuts, and
       the merge joins the unchanged content. */
    map = build_map(twelve_eight, 1);
    NoteEvent *whole = make_note("whole", 0, 1, 3, 2, PITCH_LETTER_C);
    NoteEvent *held[1] = {whole};
    assert(sonority_builder_build_adaptive(held, 1, map, standard_meter_segmentation_beat_grid, NULL, NULL,
                                           &out) == AD_SEGMENTATION_OK);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 3, 2);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    note_event_destroy(whole);
}

int main(void) {
    test_simple_meters();
    test_compound_meters();
    test_irregular_meters_are_bottom_unit_pulses();
    test_a_short_measure_ends_at_the_barline();
    test_a_pickup_measure();
    test_declines_bad_input();
    test_the_builder_cuts_6_8_and_12_8_on_dotted_quarter_beats();
    printf("test_segmentation_beat_grid: ok\n");
    return 0;
}
