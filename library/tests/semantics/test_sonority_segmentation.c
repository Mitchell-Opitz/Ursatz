#include "semantics/sonority_segmentation.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "semantics/sonority_segmentation_internal.h"
#include "time/duration.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"
#include "../rational_test_util.h"

/*
 * Adaptive sonority segmentation tests. Step 1, the grid cut: a beat boundary of the current meter
 * starts a new segment, every sounding note belongs to it, held notes are flagged held, and silence
 * makes no segment. The beat grid is injected; these tests use a stub provider that lays a fixed beat
 * length across each measure (the standard-meter provider has its own tests).
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

/* A note at onset on_n/on_d whole notes, lasting dur_n/dur_d, at MIDI pitch `midi` (sharps-only
   spelling, as the importer gives). */
static NoteEvent *make_note_with_spelling(const char *id, long on_n, unsigned long on_d, long dur_n,
                                          unsigned long dur_d, int midi, bool with_spelling) {
    static const PitchLetter LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
    static const int ACCIDENTAL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

    MusicalTime *position = NULL;
    assert(musical_time_create(on_n, on_d, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = NULL;
    assert(duration_create(dur_n, dur_d, &duration) == DURATION_OK);
    PitchIdentity *pitch = NULL;
    assert(pitch_identity_create("pitch", 6, &pitch) == PITCH_IDENTITY_OK);
    PitchSpelling *spelling = NULL;
    if (with_spelling) {
        assert(pitch_spelling_create(LETTER[midi % 12], ACCIDENTAL[midi % 12], midi / 12 - 1, &spelling) ==
               PITCH_SPELLING_OK);
    }
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent *note = NULL;
    assert(note_event_create(make_id(id), anchor, duration, pitch, spelling, NULL, provenance, &note) ==
           NOTE_EVENT_OK);
    return note;
}

static NoteEvent *make_note(const char *id, long on_n, unsigned long on_d, long dur_n, unsigned long dur_d,
                            int midi) {
    return make_note_with_spelling(id, on_n, on_d, dur_n, dur_d, midi, true);
}

static void destroy_notes(NoteEvent **notes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(notes[i]);
    }
}

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
        NoteValue *unit = NULL;
        assert(note_value_create(changes[i].bottom_exponent, &unit) == NOTE_VALUE_OK);
        assert(time_signature_create(changes[i].top, unit, &signatures[i]) == TIME_SIGNATURE_OK);
        note_value_destroy(unit);
        assert(musical_time_create(changes[i].start_n, changes[i].start_d, &starts[i]) == MUSICAL_TIME_OK);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    MeterMap *map = NULL;
    assert(meter_map_create(make_id("map"), points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    return map;
}

/* A stub beat grid: a beat is `n/d` of a whole note, laid from the start of each measure; the last beat
   of a short measure is cut at the measure's end. */
typedef struct {
    long n;
    unsigned long d;
} StubBeat;

static bool stub_provider(const MeterMap *map, int measure, void *context, MusicalTime ***out_starts,
                          size_t *out_count) {
    const StubBeat *beat = (const StubBeat *)context;
    TimeSpan *span = NULL;
    if (meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
        return false;
    }
    Rational at = test_rational_of_time(time_span_start(span));
    Rational end = test_rational_of_time(time_span_end(span));
    time_span_destroy(span);

    Rational step = rational_ratio((unsigned long long)beat->n, beat->d);
    MusicalTime **starts = (MusicalTime **)calloc(64, sizeof(MusicalTime *));
    size_t count = 0;
    while (rational_cmp(at, end) < 0 && count < 64) {
        starts[count++] = musical_time_from_rational_or_null(at);
        assert(rational_add(at, step, &at) == 0);
    }
    *out_starts = starts;
    *out_count = count;
    return true;
}

static bool failing_provider(const MeterMap *map, int measure, void *context, MusicalTime ***out_starts,
                             size_t *out_count) {
    (void)map;
    (void)measure;
    (void)context;
    *out_starts = NULL;
    *out_count = 0;
    return false;
}

static MeterMap *four_four(void) {
    Change changes[] = {{4, 2, 0, 1}};
    return build_map(changes, 1);
}

static void assert_window(const AdaptiveSonority *s, long sn, unsigned long sd, long en, unsigned long ed) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    assert(musical_time_create(sn, sd, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create(en, ed, &end) == MUSICAL_TIME_OK);
    const TimeSpan *window = sonority_time_window(s->sonority);
    assert(musical_time_equals(time_span_start(window), start));
    assert(musical_time_equals(time_span_end(window), end));
    assert(time_span_boundary(window) == TIME_SPAN_BOUNDARY_LEFT_CLOSED);
    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void assert_member(const AdaptiveSonority *s, size_t index, const char *id, bool held) {
    assert(strcmp(entity_id_value(sonority_event_id_at(s->sonority, index)), id) == 0);
    assert(s->held[index] == held);
}

static void test_a_held_whole_note_is_held_in_every_later_beat(void) {
    /* A whole note C3 under four quarter notes: it is attacked in beat 1 and held in beats 2 to 4. */
    NoteEvent *notes[5] = {
        make_note("bass", 0, 1, 1, 1, 48), make_note("q1", 0, 1, 1, 4, 64), make_note("q2", 1, 4, 1, 4, 65),
        make_note("q3", 1, 2, 1, 4, 67), make_note("q4", 3, 4, 1, 4, 69),
    };
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(notes, 5, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_OK);

    assert(out.count == 4);
    const char *upper[4] = {"q1", "q2", "q3", "q4"};
    for (size_t beat = 0; beat < 4; beat++) {
        assert(sonority_event_id_count(out.items[beat].sonority) == 2);
        assert_window(&out.items[beat], (long)beat, 4, (long)beat + 1, 4);
        assert_member(&out.items[beat], 0, "bass", beat > 0); /* attacked in beat 1, held after */
        assert_member(&out.items[beat], 1, upper[beat], false);
        assert(sonority_event_domain(out.items[beat].sonority) == SONORITY_EVENT_DOMAIN_SOUNDING);
        assert(sonority_voice_id_count(out.items[beat].sonority) == 0);
    }

    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 5);
}

static void test_silence_makes_no_segment(void) {
    /* A quarter note in beat 1 and another in beat 4: beats 2 and 3 are silent. */
    NoteEvent *notes[2] = {make_note("a", 0, 1, 1, 4, 60), make_note("b", 3, 4, 1, 4, 62)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(notes, 2, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_OK);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 4);
    assert_window(&out.items[1], 3, 4, 1, 1);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_segmentation_stops_after_the_last_sounding_note(void) {
    NoteEvent *notes[1] = {make_note("a", 0, 1, 1, 4, 60)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    NoteSpan *spans = NULL;
    SegmentList list = {0};
    assert(segmentation_prepare_notes(notes, 1, &spans) == AD_SEGMENTATION_OK);
    assert(segmentation_cut(spans, 1, map, stub_provider, &quarter, &list) == AD_SEGMENTATION_OK);
    /* The note ends at 1/4, so measure 1 is cut into its four beats (the later beats empty) and no
       further measure is started. */
    assert(list.count == 4);
    assert(list.items[0].member_count == 1 && list.items[0].starts_measure);
    assert(list.items[1].member_count == 0 && !list.items[1].starts_measure);
    segment_list_destroy(&list);
    free(spans);
    meter_map_destroy(map);
    destroy_notes(notes, 1);
}

static void test_the_grid_follows_the_meter(void) {
    /* A 6/8 measure is two beats of 3/8: the cut falls at 3/8, not on quarter notes. */
    Change changes[] = {{6, 3, 0, 1}};
    MeterMap *map = build_map(changes, 1);
    NoteEvent *notes[2] = {make_note("a", 0, 1, 3, 4, 60), make_note("b", 1, 8, 1, 8, 64)};
    StubBeat dotted_quarter = {3, 8};
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(notes, 2, map, stub_provider, &dotted_quarter, NULL, &out) ==
           AD_SEGMENTATION_OK);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 3, 8);
    assert_window(&out.items[1], 3, 8, 3 * 2, 8);
    assert(sonority_event_id_count(out.items[0].sonority) == 2);
    assert_member(&out.items[1], 0, "a", true); /* held into the second beat; b has ended */
    assert(sonority_event_id_count(out.items[1].sonority) == 1);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_a_pickup_measure_is_cut_on_its_own_span(void) {
    /* A quarter-note pickup in 4/4, then 3/4: the pickup measure is [0, 1/4), one beat. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 1, 4}};
    MeterMap *map = build_map(changes, 2);
    NoteEvent *notes[2] = {make_note("pickup", 0, 1, 1, 4, 67), make_note("down", 1, 4, 3, 4, 60)};
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(notes, 2, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_OK);
    /* The pickup measure is its own segment; the 3/4 measure's three beats hold the same note and
       merge (the merge stays inside the measure). */
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 4);
    assert_member(&out.items[0], 0, "pickup", false);
    assert_window(&out.items[1], 1, 4, 1, 1);
    assert_member(&out.items[1], 0, "down", false);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_a_short_measure_is_cut_at_its_end(void) {
    /* 4/4 cut short at 5/2 by a 3/4 change: measure 3 is [2, 5/2) and has two beats. */
    Change changes[] = {{4, 2, 0, 1}, {3, 2, 5, 2}};
    MeterMap *map = build_map(changes, 2);
    NoteEvent *notes[1] = {make_note("long", 2, 1, 3, 4, 60)}; /* [2, 11/4) */
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(notes, 1, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_OK);
    /* Measure 3's two beats hold one note and merge; the 3/4 measure after it is a new segment, so a
       merge does not cross the cut-short barline. */
    assert(out.count == 2);
    assert_window(&out.items[0], 2, 1, 5, 2);
    assert_window(&out.items[1], 5, 2, 11, 4);
    assert_member(&out.items[1], 0, "long", true);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 1);
}

static void test_rejects_bad_input(void) {
    NoteEvent *note = make_note("a", 0, 1, 1, 4, 60);
    NoteEvent *no_pitch = make_note_with_spelling("b", 0, 1, 1, 4, 60, false);
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = {NULL, 7};
    NoteEvent *one[1] = {note};
    NoteEvent *unspelled[1] = {no_pitch};
    NoteEvent *with_null[1] = {NULL};

    assert(sonority_builder_build_adaptive(NULL, 1, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    assert(sonority_builder_build_adaptive(one, 0, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    assert(sonority_builder_build_adaptive(one, 1, NULL, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    assert(sonority_builder_build_adaptive(one, 1, map, NULL, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    assert(sonority_builder_build_adaptive(with_null, 1, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_NULL_NOTE);
    assert(sonority_builder_build_adaptive(unspelled, 1, map, stub_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_NO_PITCH);
    assert(sonority_builder_build_adaptive(one, 1, map, failing_provider, &quarter, NULL, &out) ==
           AD_SEGMENTATION_ERR_BEAT_GRID);

    AdaptiveSegmentationConfig bad = adaptive_segmentation_config_default();
    bad.min_split_denominator = 0;
    assert(sonority_builder_build_adaptive(one, 1, map, stub_provider, &quarter, &bad, &out) ==
           AD_SEGMENTATION_ERR_BAD_CONFIG);
    bad.min_split_denominator = 2;
    bad.min_split_numerator = 3; /* more than a whole beat */
    assert(sonority_builder_build_adaptive(one, 1, map, stub_provider, &quarter, &bad, &out) ==
           AD_SEGMENTATION_ERR_BAD_CONFIG);

    assert(out.items == NULL && out.count == 7); /* untouched on every failure */
    adaptive_sonorities_destroy(NULL);
    meter_map_destroy(map);
    note_event_destroy(note);
    note_event_destroy(no_pitch);
}

static void test_config_defaults(void) {
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    assert(config.min_split_numerator == 1 && config.min_split_denominator == 2);
    assert(!config.merge_across_barlines);
    assert(config.merge_unchanged_harmony);
}

static void test_pitch_and_pitch_class_from_the_spelling(void) {
    NoteEvent *notes[3] = {make_note("c4", 0, 1, 1, 4, 60), make_note("fs3", 0, 1, 1, 4, 54),
                           make_note("c8", 0, 1, 1, 4, 108)};
    NoteSpan *spans = NULL;
    assert(segmentation_prepare_notes(notes, 3, &spans) == AD_SEGMENTATION_OK);
    assert(spans[0].pitch == 60 && spans[0].pitch_class == 0);
    assert(spans[1].pitch == 54 && spans[1].pitch_class == 6);
    assert(spans[2].pitch == 108 && spans[2].pitch_class == 0);

    Segment all;
    SegmentMember members[3] = {{0, true}, {1, true}, {2, true}};
    all.members = members;
    all.member_count = 3;
    assert(segment_bass_pitch(spans, &all) == 54);
    assert(segment_pitch_class_set(spans, &all) == ((1u << 0) | (1u << 6)));
    free(spans);
    destroy_notes(notes, 3);
}

/* --- step 3, the merge --- */

static AdaptiveSonorities build(NoteEvent **notes, size_t count, const MeterMap *map, StubBeat *beat,
                                const AdaptiveSegmentationConfig *config) {
    AdaptiveSonorities out = {0};
    assert(sonority_builder_build_adaptive(notes, count, map, stub_provider, beat, config, &out) ==
           AD_SEGMENTATION_OK);
    return out;
}

static void test_an_unchanged_chord_over_a_bar_merges_to_one_segment(void) {
    NoteEvent *notes[3] = {make_note("c", 0, 1, 1, 1, 48), make_note("e", 0, 1, 1, 1, 52),
                           make_note("g", 0, 1, 1, 1, 55)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 3, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 1);
    assert(sonority_event_id_count(out.items[0].sonority) == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(!out.items[0].held[i]); /* attacked in the segment: it began with them */
    }
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 3);
}

static void test_a_merge_does_not_cross_a_barline_by_default(void) {
    NoteEvent *notes[3] = {make_note("c", 0, 1, 2, 1, 48), make_note("e", 0, 1, 2, 1, 52),
                           make_note("g", 0, 1, 2, 1, 55)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};

    AdaptiveSonorities out = build(notes, 3, map, &quarter, NULL);
    assert(out.count == 2); /* one per bar */
    assert_window(&out.items[0], 0, 1, 1, 1);
    assert_window(&out.items[1], 1, 1, 2, 1);
    assert(!out.items[0].held[0] && out.items[1].held[0]); /* held into the second bar */
    adaptive_sonorities_destroy(&out);

    AdaptiveSegmentationConfig across = adaptive_segmentation_config_default();
    across.merge_across_barlines = true;
    out = build(notes, 3, map, &quarter, &across);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 2, 1);
    assert(!out.items[0].held[0]); /* attacked in the merged segment, which began with it */
    adaptive_sonorities_destroy(&out);

    meter_map_destroy(map);
    destroy_notes(notes, 3);
}

static void test_a_change_of_pitch_class_content_does_not_merge(void) {
    /* A C major triad for two beats, then a G major triad: two segments. */
    NoteEvent *notes[6] = {make_note("c", 0, 1, 1, 2, 48), make_note("e", 0, 1, 1, 2, 52),
                           make_note("g", 0, 1, 1, 2, 55), make_note("g2", 1, 2, 1, 2, 43),
                           make_note("b", 1, 2, 1, 2, 47), make_note("d", 1, 2, 1, 2, 50)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 6, map, &quarter, NULL);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 2);
    assert_window(&out.items[1], 1, 2, 1, 1);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 6);
}

static void test_the_same_pitch_classes_over_a_different_bass_do_not_merge(void) {
    /* C E G with C in the bass, then C E G with E in the bass. */
    NoteEvent *notes[6] = {make_note("c3", 0, 1, 1, 2, 48), make_note("e4", 0, 1, 1, 2, 64),
                           make_note("g4", 0, 1, 1, 2, 67), make_note("e3", 1, 2, 1, 2, 52),
                           make_note("c4", 1, 2, 1, 2, 60), make_note("g4b", 1, 2, 1, 2, 67)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 6, map, &quarter, NULL);
    assert(out.count == 2);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 6);
}

static void test_the_bass_in_a_different_octave_still_merges(void) {
    /* The bass is C3 for a beat and C2 for the next: the same pitch class, so the beats merge. */
    NoteEvent *notes[4] = {make_note("c3", 0, 1, 1, 4, 48), make_note("e4a", 0, 1, 1, 4, 64),
                           make_note("c2", 1, 4, 1, 4, 36), make_note("e4b", 1, 4, 1, 4, 64)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 4, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 2);
    assert(sonority_event_id_count(out.items[0].sonority) == 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

static void test_silence_keeps_equal_segments_apart(void) {
    NoteEvent *notes[2] = {make_note("a", 0, 1, 1, 4, 60), make_note("b", 1, 2, 1, 4, 60)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 2, map, &quarter, NULL);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 4);
    assert_window(&out.items[1], 1, 2, 3, 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_a_merged_segment_holds_each_note_once_and_keeps_attacks(void) {
    /* A whole-note bass C3 with E4 in beat 1 and E4 again in beat 2: the sets match, so beats 1-2 merge
       into one segment holding the bass once, attacked, and both E4s. Beat 3 adds a G4: a new set. */
    NoteEvent *notes[4] = {make_note("bass", 0, 1, 1, 1, 48), make_note("e1", 0, 1, 1, 4, 64),
                           make_note("e2", 1, 4, 1, 4, 64), make_note("g", 1, 2, 1, 2, 67)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 4, map, &quarter, NULL);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 2);
    assert(sonority_event_id_count(out.items[0].sonority) == 3);
    assert_member(&out.items[0], 0, "bass", false);
    assert_member(&out.items[0], 1, "e1", false);
    assert_member(&out.items[0], 2, "e2", false);
    assert_window(&out.items[1], 1, 2, 1, 1);
    assert_member(&out.items[1], 0, "bass", true);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

static void test_arpeggiation_within_a_beat_merges_only_when_content_matches(void) {
    /* A sustained arpeggio: C is held while E and G are added, so the bass stays C (no split). Beat 1
       sounds C E G and so does beat 2 (the arpeggio again): merged. */
    NoteEvent *same[6] = {make_note("a1", 0, 1, 1, 4, 48), make_note("a2", 1, 16, 3, 16, 52),
                          make_note("a3", 1, 8, 1, 8, 55), make_note("b1", 1, 4, 1, 4, 48),
                          make_note("b2", 5, 16, 3, 16, 52), make_note("b3", 3, 8, 1, 8, 55)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(same, 6, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 2);
    adaptive_sonorities_destroy(&out);
    destroy_notes(same, 6);

    /* C E in beat 1 and E G in beat 2 (E is the bass of beat 2): different sets, not merged. */
    NoteEvent *different[4] = {make_note("c", 0, 1, 1, 4, 48), make_note("e", 1, 8, 1, 8, 52),
                               make_note("e2", 1, 4, 1, 4, 52), make_note("g", 3, 8, 1, 8, 55)};
    out = build(different, 4, map, &quarter, NULL);
    assert(out.count == 2);
    adaptive_sonorities_destroy(&out);
    destroy_notes(different, 4);
    meter_map_destroy(map);
}

static void test_merge_edits_the_segment_list_in_place(void) {
    NoteEvent *notes[1] = {make_note("a", 0, 1, 1, 1, 60)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    NoteSpan *spans = NULL;
    SegmentList list = {0};
    assert(segmentation_prepare_notes(notes, 1, &spans) == AD_SEGMENTATION_OK);
    assert(segmentation_cut(spans, 1, map, stub_provider, &quarter, &list) == AD_SEGMENTATION_OK);
    assert(list.count == 4);
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    assert(segmentation_merge(spans, &config, &list) == AD_SEGMENTATION_OK);
    assert(list.count == 1);
    assert(list.items[0].starts_measure && list.items[0].member_count == 1);
    assert(segmentation_merge(NULL, &config, &list) == AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    segment_list_destroy(&list);
    free(spans);
    meter_map_destroy(map);
    destroy_notes(notes, 1);
}

/* --- step 4, the merge over unchanged harmony --- */

static AdaptiveSegmentationConfig without_harmony_merge(void) {
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    config.merge_unchanged_harmony = false;
    return config;
}

static void test_a_melody_note_over_unchanged_harmony_merges(void) {
    /* C3 and G3 held for two beats, E4 then F4 above them: the F4 is a new attack above the bass, and C and G
       still sound, so the two beats are one segment. Without the step they are two. */
    NoteEvent *notes[4] = {make_note("c", 0, 1, 1, 2, 48), make_note("g", 0, 1, 1, 2, 55),
                           make_note("e", 0, 1, 1, 4, 64), make_note("f", 1, 4, 1, 4, 65)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 4, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 2);
    assert(sonority_event_id_count(out.items[0].sonority) == 4);
    adaptive_sonorities_destroy(&out);

    AdaptiveSegmentationConfig off = without_harmony_merge();
    out = build(notes, 4, map, &quarter, &off);
    assert(out.count == 2);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

static void test_a_run_of_melody_notes_merges_against_the_first_chord(void) {
    /* C3, E3 and G3 held for a bar under E4 F4 G4 A4: every beat holds the chord, so one segment. */
    NoteEvent *notes[7] = {make_note("c", 0, 1, 1, 1, 48), make_note("e3", 0, 1, 1, 1, 52),
                           make_note("g", 0, 1, 1, 1, 55), make_note("m0", 0, 1, 1, 4, 64),
                           make_note("m1", 1, 4, 1, 4, 65), make_note("m2", 1, 2, 1, 4, 67),
                           make_note("m3", 3, 4, 1, 4, 69)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 7, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 1);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 7);
}

static void test_a_new_bass_attack_does_not_merge_over_harmony(void) {
    /* C3 and G3 held for two beats with E4 over them, then a new A1 attacked in beat 2: a bass attack. */
    NoteEvent *notes[4] = {make_note("c", 0, 1, 1, 2, 48), make_note("g", 0, 1, 1, 2, 55),
                           make_note("e", 0, 1, 1, 2, 64), make_note("low", 1, 4, 1, 4, 33)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 4, map, &quarter, NULL);
    assert(out.count == 2);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

static void test_a_released_chord_tone_does_not_merge(void) {
    /* C3 for two beats, G3 for one, E4 then F4 above: in beat 2 only C is held, so the chord has changed. */
    NoteEvent *notes[4] = {make_note("c", 0, 1, 1, 2, 48), make_note("g", 0, 1, 1, 4, 55),
                           make_note("e", 0, 1, 1, 4, 64), make_note("f", 1, 4, 1, 4, 65)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 4, map, &quarter, NULL);
    assert(out.count == 2);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

static void test_the_harmony_merge_does_not_cross_a_barline_by_default(void) {
    /* C3 and G3 held over the barline, E4 in bar 1 and F4 in bar 2. */
    NoteEvent *notes[4] = {make_note("c", 0, 1, 2, 1, 48), make_note("g", 0, 1, 2, 1, 55),
                           make_note("e", 0, 1, 1, 1, 64), make_note("f", 1, 1, 1, 1, 65)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 4, map, &quarter, NULL);
    assert(out.count == 2);
    adaptive_sonorities_destroy(&out);
    AdaptiveSegmentationConfig across = adaptive_segmentation_config_default();
    across.merge_across_barlines = true;
    out = build(notes, 4, map, &quarter, &across);
    assert(out.count == 1);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

static void test_the_harmony_merge_edits_the_segment_list_in_place(void) {
    NoteEvent *notes[4] = {make_note("c", 0, 1, 1, 2, 48), make_note("g", 0, 1, 1, 2, 55),
                           make_note("e", 0, 1, 1, 4, 64), make_note("f", 1, 4, 1, 4, 65)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    NoteSpan *spans = NULL;
    SegmentList list = {0};
    assert(segmentation_prepare_notes(notes, 4, &spans) == AD_SEGMENTATION_OK);
    assert(segmentation_cut(spans, 4, map, stub_provider, &quarter, &list) == AD_SEGMENTATION_OK);
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    assert(segmentation_merge(spans, &config, &list) == AD_SEGMENTATION_OK);
    size_t before = list.count;
    AdaptiveSegmentationConfig off = without_harmony_merge();
    assert(segmentation_merge_unchanged_harmony(spans, &off, &list) == AD_SEGMENTATION_OK);
    assert(list.count == before); /* off: untouched */
    assert(segmentation_merge_unchanged_harmony(spans, &config, &list) == AD_SEGMENTATION_OK);
    assert(list.count == before - 1);
    assert(list.items[0].member_count == 4);
    assert(segmentation_merge_unchanged_harmony(NULL, &config, &list) == AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    segment_list_destroy(&list);
    free(spans);
    meter_map_destroy(map);
    destroy_notes(notes, 4);
}

/* --- step 2, the bass split --- (beats are quarter notes; the default minimum is half a beat, 1/8) */

static void test_a_bass_change_mid_beat_splits(void) {
    /* Bass C3 for the first half of beat 1 and E3 for the second, over a held G4. */
    NoteEvent *notes[3] = {make_note("c3", 0, 1, 1, 8, 48), make_note("e3", 1, 8, 1, 8, 52),
                           make_note("g4", 0, 1, 1, 4, 67)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 3, map, &quarter, NULL);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 8);
    assert(sonority_event_id_count(out.items[0].sonority) == 2);
    assert_member(&out.items[0], 0, "c3", false);
    assert_member(&out.items[0], 1, "g4", false);
    assert_window(&out.items[1], 1, 8, 1, 4); /* at the attack of the new bass */
    assert(sonority_event_id_count(out.items[1].sonority) == 2);
    assert_member(&out.items[1], 0, "e3", false);
    assert_member(&out.items[1], 1, "g4", true); /* held across the split */
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 3);
}

static void test_a_bass_change_shorter_than_the_minimum_does_not_split(void) {
    /* E3 arrives at 3/16 and lasts 1/16, under the 1/8 minimum: the beat stays whole. */
    NoteEvent *notes[2] = {make_note("c3", 0, 1, 3, 16, 48), make_note("e3", 3, 16, 1, 16, 52)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 2, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 4);
    assert(sonority_event_id_count(out.items[0].sonority) == 2);
    adaptive_sonorities_destroy(&out);

    /* The minimum is configurable: at a quarter of a beat (1/16) the same change splits. */
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    config.min_split_numerator = 1;
    config.min_split_denominator = 4;
    out = build(notes, 2, map, &quarter, &config);
    assert(out.count == 2);
    assert_window(&out.items[1], 3, 16, 1, 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_the_new_bass_is_measured_to_the_next_bass_change(void) {
    /* C3 [0,1/16), D3 [1/16,1/8), E3 [1/8,1/4): D lasts only until E arrives (1/16, too short), so it is
       not a split; E lasts to the end of the beat (1/8) and is. */
    NoteEvent *notes[3] = {make_note("c3", 0, 1, 1, 16, 48), make_note("d3", 1, 16, 1, 16, 50),
                           make_note("e3", 1, 8, 1, 8, 52)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 3, map, &quarter, NULL);
    assert(out.count == 2);
    assert_window(&out.items[0], 0, 1, 1, 8);
    assert(sonority_event_id_count(out.items[0].sonority) == 2); /* C and D together */
    assert_window(&out.items[1], 1, 8, 1, 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 3);
}

static void test_an_octave_change_of_the_bass_does_not_split(void) {
    NoteEvent *notes[2] = {make_note("c3", 0, 1, 1, 8, 48), make_note("c2", 1, 8, 1, 8, 36)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 2, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_a_bass_that_changes_by_release_alone_does_not_split(void) {
    /* E3 sounds all beat; C3 below it releases at 1/8, so E3 becomes the lowest without an attack. */
    NoteEvent *notes[2] = {make_note("c3", 0, 1, 1, 8, 48), make_note("e3", 0, 1, 1, 4, 52)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 2, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 0, 1, 1, 4);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_a_higher_note_attacked_over_the_bass_does_not_split(void) {
    NoteEvent *notes[2] = {make_note("c3", 0, 1, 1, 4, 48), make_note("g4", 1, 8, 1, 8, 67)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 2, map, &quarter, NULL);
    assert(out.count == 1);
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

static void test_a_bass_attack_after_silence_in_the_beat_starts_the_segment(void) {
    NoteEvent *notes[1] = {make_note("c3", 1, 8, 1, 8, 48)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    AdaptiveSonorities out = build(notes, 1, map, &quarter, NULL);
    assert(out.count == 1);
    assert_window(&out.items[0], 1, 8, 1, 4); /* not [0, 1/4): the silence before the attack is cut off */
    adaptive_sonorities_destroy(&out);
    meter_map_destroy(map);
    destroy_notes(notes, 1);
}

static void test_a_split_keeps_the_barline_flag_on_its_first_piece(void) {
    NoteEvent *notes[2] = {make_note("c3", 0, 1, 1, 8, 48), make_note("e3", 1, 8, 1, 8, 52)};
    MeterMap *map = four_four();
    StubBeat quarter = {1, 4};
    NoteSpan *spans = NULL;
    SegmentList list = {0};
    assert(segmentation_prepare_notes(notes, 2, &spans) == AD_SEGMENTATION_OK);
    assert(segmentation_cut(spans, 2, map, stub_provider, &quarter, &list) == AD_SEGMENTATION_OK);
    assert(list.count == 4);
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    assert(segmentation_split_bass(spans, &config, &list) == AD_SEGMENTATION_OK);
    assert(list.count == 5); /* beat 1 became two pieces; beats 2 to 4 are untouched and empty */
    assert(list.items[0].starts_measure && !list.items[1].starts_measure);
    assert(list.items[0].member_count == 1 && list.items[1].member_count == 1);
    assert(segmentation_split_bass(NULL, &config, &list) == AD_SEGMENTATION_ERR_NULL_ARGUMENT);
    segment_list_destroy(&list);
    free(spans);
    meter_map_destroy(map);
    destroy_notes(notes, 2);
}

int main(void) {
    test_a_held_whole_note_is_held_in_every_later_beat();
    test_silence_makes_no_segment();
    test_segmentation_stops_after_the_last_sounding_note();
    test_the_grid_follows_the_meter();
    test_a_pickup_measure_is_cut_on_its_own_span();
    test_a_short_measure_is_cut_at_its_end();
    test_rejects_bad_input();
    test_config_defaults();
    test_pitch_and_pitch_class_from_the_spelling();
    test_an_unchanged_chord_over_a_bar_merges_to_one_segment();
    test_a_merge_does_not_cross_a_barline_by_default();
    test_a_change_of_pitch_class_content_does_not_merge();
    test_the_same_pitch_classes_over_a_different_bass_do_not_merge();
    test_the_bass_in_a_different_octave_still_merges();
    test_silence_keeps_equal_segments_apart();
    test_a_merged_segment_holds_each_note_once_and_keeps_attacks();
    test_arpeggiation_within_a_beat_merges_only_when_content_matches();
    test_merge_edits_the_segment_list_in_place();
    test_a_melody_note_over_unchanged_harmony_merges();
    test_a_run_of_melody_notes_merges_against_the_first_chord();
    test_a_new_bass_attack_does_not_merge_over_harmony();
    test_a_released_chord_tone_does_not_merge();
    test_the_harmony_merge_does_not_cross_a_barline_by_default();
    test_the_harmony_merge_edits_the_segment_list_in_place();
    test_a_bass_change_mid_beat_splits();
    test_a_bass_change_shorter_than_the_minimum_does_not_split();
    test_the_new_bass_is_measured_to_the_next_bass_change();
    test_an_octave_change_of_the_bass_does_not_split();
    test_a_bass_that_changes_by_release_alone_does_not_split();
    test_a_higher_note_attacked_over_the_bass_does_not_split();
    test_a_bass_attack_after_silence_in_the_beat_starts_the_segment();
    test_a_split_keeps_the_barline_flag_on_its_first_piece();
    printf("test_sonority_segmentation: ok\n");
    return 0;
}
