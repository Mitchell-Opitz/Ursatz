#include "analysis/outer_voice.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice_internal.h"
#include "core/provenance_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"
#include "../rational_test_util.h"

/*
 * Outer voice extraction tests: the highest and lowest sounding note of each segment, absent entries kept
 * distinct from a note that merely happens to be highest, doublings, an Alberti bass, and bad input. The
 * smoothing rules have their own tests (below). Notes are built with MIDI pitches under one tuning system.
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static TuningSystem *make_tuning(const char *id) {
    TuningSystem *tuning = NULL;
    assert(tuning_system_create(make_id(id), "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    return tuning;
}

typedef struct {
    NoteEvent *notes[32];
    PitchNumericValue *values[32];
    size_t count;
} Notes;

static void add_note_in(Notes *set, const TuningSystem *tuning, const char *id, long on_n, unsigned long on_d, long dur_n,
                        unsigned long dur_d, int midi) {
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
    assert(pitch_spelling_create(LETTER[midi % 12], ACCIDENTAL[midi % 12], midi / 12 - 1, &spelling) ==
           PITCH_SPELLING_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(note_event_create(make_id(id), anchor, duration, pitch, spelling, NULL, provenance,
                             &set->notes[set->count]) == NOTE_EVENT_OK);
    assert(pitch_numeric_value_create(tuning, midi, 1, &set->values[set->count]) == PITCH_NUMERIC_VALUE_OK);
    set->count++;
}

static void destroy_notes(Notes *set) {
    for (size_t i = 0; i < set->count; i++) {
        note_event_destroy(set->notes[i]);
        pitch_numeric_value_destroy(set->values[i]);
    }
}

/* A 4/4 map and a stub quarter-note beat grid, as the sonority segmentation tests use. */
static MeterMap *four_four(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create(4, quarter, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {signature, zero};
    MeterMap *map = NULL;
    assert(meter_map_create(make_id("map"), &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(signature);
    musical_time_destroy(zero);
    return map;
}

static bool quarter_grid(const MeterMap *map, int measure, void *context, MusicalTime ***out_starts, size_t *out_count) {
    (void)context;
    TimeSpan *span = NULL;
    if (meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
        return false;
    }
    Rational at = test_rational_of_time(time_span_start(span));
    Rational end = test_rational_of_time(time_span_end(span));
    time_span_destroy(span);
    MusicalTime **starts = (MusicalTime **)calloc(64, sizeof(MusicalTime *));
    size_t count = 0;
    while (rational_cmp(at, end) < 0 && count < 64) {
        starts[count++] = musical_time_from_rational_or_null(at);
        assert(rational_add(at, rational_ratio(1, 4), &at) == 0);
    }
    *out_starts = starts;
    *out_count = count;
    return true;
}

static OuterVoiceConfig raw_config(void) {
    OuterVoiceConfig config = outer_voice_config_default();
    config.bridge_gaps = false;
    config.guard_soprano_register = false;
    config.guard_bass_register = false;
    return config;
}

/* One segment per melody note: the merge over unchanged harmony is off, as for every outer-voice caller. */
static AdaptiveSegmentationConfig voice_segmentation(void) {
    AdaptiveSegmentationConfig config = adaptive_segmentation_config_default();
    config.merge_unchanged_harmony = false;
    return config;
}

/* Segments the notes on the quarter-note grid and extracts both lines. */
static void extract(Notes *set, const MeterMap *map, const OuterVoiceConfig *config, AdaptiveSonorities *segments,
                    OuterVoiceLines *lines) {
    AdaptiveSegmentationConfig segmentation = voice_segmentation();
    assert(sonority_builder_build_adaptive(set->notes, set->count, map, quarter_grid, NULL, &segmentation, segments) ==
           AD_SEGMENTATION_OK);
    assert(outer_voice_extract(segments, set->notes, set->values, set->count, map, quarter_grid, NULL, config,
                               lines) == OUTER_VOICE_OK);
}

static void assert_entry(const OuterVoiceEntry *entry, const char *id, long pitch) {
    assert(entry->present);
    assert(strcmp(entity_id_value(entry->note_id), id) == 0);
    assert(pitch_numeric_value_position_numerator(entry->pitch) == pitch);
}

static void assert_span(const OuterVoiceEntry *entry, long sn, unsigned long sd, long en, unsigned long ed) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    assert(musical_time_create(sn, sd, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create(en, ed, &end) == MUSICAL_TIME_OK);
    assert(musical_time_equals(time_span_start(entry->span), start));
    assert(musical_time_equals(time_span_end(entry->span), end));
    musical_time_destroy(start);
    musical_time_destroy(end);
}

static void test_a_single_voice_is_both_outer_lines(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "n0", 0, 1, 1, 4, 60);
    add_note_in(&set, tuning, "n1", 1, 4, 1, 4, 62);
    add_note_in(&set, tuning, "n2", 1, 2, 1, 4, 64);
    add_note_in(&set, tuning, "n3", 3, 4, 1, 4, 65);
    MeterMap *map = four_four();
    OuterVoiceConfig config = raw_config();
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&set, map, &config, &segments, &lines);

    assert(lines.count == 4);
    const char *ids[4] = {"n0", "n1", "n2", "n3"};
    long pitches[4] = {60, 62, 64, 65};
    for (size_t i = 0; i < 4; i++) {
        assert_entry(&lines.soprano[i], ids[i], pitches[i]);
        assert_entry(&lines.bass[i], ids[i], pitches[i]);
        assert(lines.soprano[i].segment_index == i && !lines.soprano[i].bridged && !lines.soprano[i].guarded);
        assert_span(&lines.soprano[i], (long)i, 4, (long)i + 1, 4);
    }

    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_a_melody_over_block_chords(void) {
    /* A whole-note C3 E3 G3 chord under a melody E5 F5 G5 A5: the soprano is the melody, the bass the C3. */
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "c", 0, 1, 1, 1, 48);
    add_note_in(&set, tuning, "e", 0, 1, 1, 1, 52);
    add_note_in(&set, tuning, "g", 0, 1, 1, 1, 55);
    add_note_in(&set, tuning, "m0", 0, 1, 1, 4, 76);
    add_note_in(&set, tuning, "m1", 1, 4, 1, 4, 77);
    add_note_in(&set, tuning, "m2", 1, 2, 1, 4, 79);
    add_note_in(&set, tuning, "m3", 3, 4, 1, 4, 81);
    MeterMap *map = four_four();
    OuterVoiceConfig config = raw_config();
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&set, map, &config, &segments, &lines);

    assert(lines.count == 4);
    const char *melody[4] = {"m0", "m1", "m2", "m3"};
    long melody_pitch[4] = {76, 77, 79, 81};
    for (size_t i = 0; i < 4; i++) {
        assert_entry(&lines.soprano[i], melody[i], melody_pitch[i]);
        assert_entry(&lines.bass[i], "c", 48);
    }

    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_a_doubled_top_note_picks_the_most_recent_attack_then_the_lowest_id(void) {
    TuningSystem *tuning = make_tuning("midi");
    MeterMap *map = four_four();
    OuterVoiceConfig config = raw_config();

    /* E5 attacked at 0 and again at 1/4 while the first still sounds: the later attack is picked. */
    Notes recent = {{0}, {0}, 0};
    add_note_in(&recent, tuning, "old", 0, 1, 1, 2, 76);
    add_note_in(&recent, tuning, "new", 1, 4, 1, 4, 76);
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&recent, map, &config, &segments, &lines);
    assert(lines.count == 1); /* the same pitch class and bass over both beats: one segment */
    assert_entry(&lines.soprano[0], "new", 76);
    assert_entry(&lines.bass[0], "new", 76); /* and the same for the bass of a doubled lowest note */
    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    destroy_notes(&recent);

    /* Two E5s attacked together: the lowest note id. */
    Notes together = {{0}, {0}, 0};
    add_note_in(&together, tuning, "b-note", 0, 1, 1, 4, 76);
    add_note_in(&together, tuning, "a-note", 0, 1, 1, 4, 76);
    extract(&together, map, &config, &segments, &lines);
    assert(lines.count == 1);
    assert_entry(&lines.soprano[0], "a-note", 76);
    assert_entry(&lines.bass[0], "a-note", 76);
    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    destroy_notes(&together);

    meter_map_destroy(map);
    tuning_system_destroy(tuning);
}

static void test_an_alberti_bass_has_the_lowest_note_of_each_segment_as_its_bass(void) {
    /* Sixteenths C3 G3 E3 G3 under a melody: the bass is the lowest note sounding in the beat, C3. */
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "a0", 0, 1, 1, 16, 48);
    add_note_in(&set, tuning, "a1", 1, 16, 1, 16, 55);
    add_note_in(&set, tuning, "a2", 1, 8, 1, 16, 52);
    add_note_in(&set, tuning, "a3", 3, 16, 1, 16, 55);
    add_note_in(&set, tuning, "m0", 0, 1, 1, 4, 72);
    add_note_in(&set, tuning, "b0", 1, 4, 1, 16, 43); /* the pattern restarts on G2 in beat 2 */
    add_note_in(&set, tuning, "b1", 5, 16, 1, 16, 50);
    add_note_in(&set, tuning, "b2", 3, 8, 1, 16, 47);
    add_note_in(&set, tuning, "b3", 7, 16, 1, 16, 50);
    add_note_in(&set, tuning, "m1", 1, 4, 1, 4, 74);
    MeterMap *map = four_four();
    OuterVoiceConfig config = raw_config();
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&set, map, &config, &segments, &lines);

    assert(lines.count == 2);
    assert_entry(&lines.bass[0], "a0", 48);
    assert_entry(&lines.soprano[0], "m0", 72);
    assert_entry(&lines.bass[1], "b0", 43);
    assert_entry(&lines.soprano[1], "m1", 74);

    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_silence_between_segments_makes_no_entry(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "a", 0, 1, 1, 4, 60);
    add_note_in(&set, tuning, "b", 1, 2, 1, 4, 62);
    MeterMap *map = four_four();
    OuterVoiceConfig config = raw_config();
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&set, map, &config, &segments, &lines);
    assert(lines.count == 2); /* beat 2 is silent: no segment, so no entry */
    assert_span(&lines.soprano[0], 0, 1, 1, 4);
    assert_span(&lines.soprano[1], 1, 2, 3, 4);
    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_pick_line_directly(void) {
    /* Three notes: a low one, a high one, and a doubling of the high one attacked later. An empty segment
       is absent. */
    OuterNote notes[3];
    memset(notes, 0, sizeof(notes));
    notes[0].id = "low";
    notes[0].onset = rational_ratio(0, 1);
    notes[0].pitch = rational_ratio(48, 1);
    notes[1].id = "high";
    notes[1].onset = rational_ratio(0, 1);
    notes[1].pitch = rational_ratio(72, 1);
    notes[2].id = "high-later";
    notes[2].onset = rational_ratio(1, 4);
    notes[2].pitch = rational_ratio(72, 1);
    size_t all[3] = {0, 1, 2};
    size_t first_two[2] = {0, 1};
    OuterSegment segments[3] = {{rational_ratio(0, 1), rational_ratio(1, 4), first_two, 2},
                                {rational_ratio(1, 4), rational_ratio(1, 2), all, 3},
                                {rational_ratio(1, 2), rational_ratio(3, 4), NULL, 0}};
    OuterNode soprano[3];
    OuterNode bass[3];
    outer_voice_pick_line(notes, segments, 3, true, soprano);
    outer_voice_pick_line(notes, segments, 3, false, bass);

    assert(soprano[0].present && soprano[0].note == 1);
    assert(soprano[1].present && soprano[1].note == 2); /* the later attack of the doubled pitch */
    assert(!soprano[2].present && !bass[2].present);
    assert(bass[0].present && bass[0].note == 0 && bass[1].present && bass[1].note == 0);
}

static void test_rejects_bad_input(void) {
    TuningSystem *tuning = make_tuning("midi");
    TuningSystem *other = make_tuning("other");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "a", 0, 1, 1, 4, 60);
    add_note_in(&set, tuning, "b", 1, 4, 1, 4, 62);
    MeterMap *map = four_four();
    AdaptiveSonorities segments = {0};
    AdaptiveSegmentationConfig segmentation = voice_segmentation();
    assert(sonority_builder_build_adaptive(set.notes, set.count, map, quarter_grid, NULL, &segmentation, &segments) ==
           AD_SEGMENTATION_OK);
    OuterVoiceLines lines = {NULL, NULL, 7};

    assert(outer_voice_extract(NULL, set.notes, set.values, 2, map, quarter_grid, NULL, NULL, &lines) ==
           OUTER_VOICE_ERR_NULL_ARGUMENT);
    assert(outer_voice_extract(&segments, NULL, set.values, 2, map, quarter_grid, NULL, NULL, &lines) ==
           OUTER_VOICE_ERR_NULL_ARGUMENT);

    OuterVoiceConfig bad = outer_voice_config_default();
    bad.gap_bridge_max_denominator = 0;
    assert(outer_voice_extract(&segments, set.notes, set.values, 2, map, quarter_grid, NULL, &bad, &lines) ==
           OUTER_VOICE_ERR_BAD_CONFIG);
    bad = outer_voice_config_default();
    bad.register_guard_semitones = -1;
    assert(outer_voice_extract(&segments, set.notes, set.values, 2, map, quarter_grid, NULL, &bad, &lines) ==
           OUTER_VOICE_ERR_BAD_CONFIG);

    /* A segment member that is not among the notes handed in. */
    assert(outer_voice_extract(&segments, set.notes, set.values, 1, map, quarter_grid, NULL, NULL, &lines) ==
           OUTER_VOICE_ERR_UNKNOWN_NOTE);

    /* Notes in two tuning systems cannot be compared. */
    PitchNumericValue *mixed[2] = {set.values[0], NULL};
    assert(pitch_numeric_value_create(other, 62, 1, &mixed[1]) == PITCH_NUMERIC_VALUE_OK);
    assert(outer_voice_extract(&segments, set.notes, mixed, 2, map, quarter_grid, NULL, NULL, &lines) ==
           OUTER_VOICE_ERR_MIXED_TUNING);
    pitch_numeric_value_destroy(mixed[1]);

    assert(lines.soprano == NULL && lines.count == 7); /* untouched on every failure */
    outer_voice_lines_destroy(NULL);
    adaptive_sonorities_destroy(&segments);
    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
    tuning_system_destroy(other);
}

static void test_config_defaults(void) {
    OuterVoiceConfig config = outer_voice_config_default();
    assert(config.gap_bridge_max_numerator == 1 && config.gap_bridge_max_denominator == 1);
    assert(config.register_guard_semitones == 12);
    assert(config.bridge_gaps && config.guard_soprano_register && config.guard_bass_register);
}

/* --- the smoothing rules, each on its own (a constant quarter-note beat) --- */

static bool constant_beat(void *context, Rational at, Rational *out) {
    (void)context;
    (void)at;
    *out = rational_ratio(1, 4);
    return true;
}

/* A note [onset, release) in 1/16s of a whole note at pitch p. */
static OuterNote note16(const char *id, unsigned long onset, unsigned long release, unsigned long pitch) {
    OuterNote n;
    memset(&n, 0, sizeof(n));
    n.id = id;
    n.onset = rational_ratio(onset, 16);
    n.release = rational_ratio(release, 16);
    n.pitch = rational_ratio(pitch, 1);
    return n;
}

/* A one-beat (4/16) segment starting at beat `beat` (0-based). */
static OuterSegment beat_segment(unsigned long beat, const size_t *members, size_t count) {
    OuterSegment s;
    s.start = rational_ratio(beat * 4, 16);
    s.end = rational_ratio(beat * 4 + 4, 16);
    s.members = members;
    s.member_count = count;
    return s;
}

static void test_the_soprano_guard_keeps_a_resting_melody_absent(void) {
    /* Melody C6 in beat 0; in beat 1 only the accompaniment (E4) sounds, 20 semitones lower, and the melody note
       ended as the beat began: absent, not the accompaniment's top note. Beat 2 the melody returns. */
    OuterNote notes[4] = {note16("m0", 0, 4, 84), note16("acc0", 0, 12, 52), note16("acc1", 4, 8, 64),
                          note16("m2", 8, 12, 86)};
    size_t beat0[2] = {0, 1};
    size_t beat1[2] = {1, 2};
    size_t beat2[2] = {1, 3};
    OuterSegment segments[3] = {beat_segment(0, beat0, 2), beat_segment(1, beat1, 2), beat_segment(2, beat2, 2)};
    OuterNode nodes[3];
    outer_voice_pick_line(notes, segments, 3, true, nodes);
    assert(nodes[1].present && notes[nodes[1].note].pitch.num == 64); /* before the guard: the accompaniment */

    assert(outer_voice_guard_register(notes, segments, 3, true, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[0].present && !nodes[0].guarded);
    assert(!nodes[1].present && nodes[1].guarded);
    assert(nodes[2].present && notes[nodes[2].note].pitch.num == 86);
}

static void test_the_soprano_guard_does_not_fire_without_a_big_enough_drop_or_a_short_enough_gap(void) {
    /* A drop of exactly an octave is not more than an octave. */
    OuterNote exact[2] = {note16("a", 0, 4, 72), note16("b", 4, 8, 60)};
    size_t first[1] = {0};
    size_t second[1] = {1};
    OuterSegment two[2] = {beat_segment(0, first, 1), beat_segment(1, second, 1)};
    OuterNode nodes[2];
    outer_voice_pick_line(exact, two, 2, true, nodes);
    assert(outer_voice_guard_register(exact, two, 2, true, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[1].present);

    /* A big drop after a gap of a whole beat or more: the line has been silent long enough, so the lower
       note is adopted (a new phrase low in the range). */
    OuterNote late[2] = {note16("a", 0, 4, 84), note16("b", 8, 12, 60)};
    OuterSegment far[2] = {beat_segment(0, first, 1), beat_segment(2, second, 1)};
    outer_voice_pick_line(late, far, 2, true, nodes);
    assert(outer_voice_guard_register(late, far, 2, true, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[1].present && !nodes[1].guarded);

    /* The previous note still sounding is not "ended": nothing to guard against. */
    OuterNote sounding[2] = {note16("a", 0, 12, 84), note16("b", 4, 8, 60)};
    OuterSegment overlapped[2] = {beat_segment(0, first, 1), beat_segment(1, second, 1)};
    outer_voice_pick_line(sounding, overlapped, 2, true, nodes);
    assert(outer_voice_guard_register(sounding, overlapped, 2, true, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[1].present);

    /* The threshold is configurable: a 20 semitone drop is under a 24 semitone guard. */
    OuterNote big[2] = {note16("a", 0, 4, 84), note16("b", 4, 8, 64)};
    outer_voice_pick_line(big, two, 2, true, nodes);
    assert(outer_voice_guard_register(big, two, 2, true, 24, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[1].present);
}

static void test_the_soprano_stays_absent_until_the_window_passes(void) {
    /* Melody C6 ends at 4/16; accompaniment-only beats follow at 1, 2 and 3. A beat is 4/16, so the window is
       a beat from the melody's end: beat 1 starts at the end (gap 0), beat 2 a beat later (gap 4/16, not under
       a beat). Only the first is absent. */
    OuterNote notes[3] = {note16("m", 0, 4, 84), note16("acc1", 4, 8, 60), note16("acc2", 8, 12, 60)};
    size_t a[1] = {0};
    size_t b[1] = {1};
    size_t c[1] = {2};
    OuterSegment segments[3] = {beat_segment(0, a, 1), beat_segment(1, b, 1), beat_segment(2, c, 1)};
    OuterNode nodes[3];
    outer_voice_pick_line(notes, segments, 3, true, nodes);
    assert(outer_voice_guard_register(notes, segments, 3, true, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(!nodes[1].present && nodes[1].guarded);
    assert(nodes[2].present && !nodes[2].guarded);

    /* A wider window (two beats) keeps beat 2 absent too. */
    outer_voice_pick_line(notes, segments, 3, true, nodes);
    assert(outer_voice_guard_register(notes, segments, 3, true, 12, rational_ratio(2, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(!nodes[1].present && !nodes[2].present);
}

static void test_the_bass_guard_mirrors_the_soprano_guard(void) {
    /* Bass C2 in beat 0; in beat 1 the lowest sounding note is C4, 24 above, as the bass note ended. */
    OuterNote notes[3] = {note16("b0", 0, 4, 36), note16("up", 0, 12, 76), note16("c4", 4, 8, 60)};
    size_t beat0[2] = {0, 1};
    size_t beat1[2] = {1, 2};
    OuterSegment segments[2] = {beat_segment(0, beat0, 2), beat_segment(1, beat1, 2)};
    OuterNode nodes[2];
    outer_voice_pick_line(notes, segments, 2, false, nodes);
    assert(nodes[1].present && notes[nodes[1].note].pitch.num == 60);
    assert(outer_voice_guard_register(notes, segments, 2, false, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[0].present);
    assert(!nodes[1].present && nodes[1].guarded);

    /* A bass that falls is not guarded (the mirror of a soprano that rises). */
    OuterNote falling[2] = {note16("a", 0, 4, 60), note16("b", 4, 8, 36)};
    size_t one[1] = {0};
    size_t two[1] = {1};
    OuterSegment pair[2] = {beat_segment(0, one, 1), beat_segment(1, two, 1)};
    outer_voice_pick_line(falling, pair, 2, false, nodes);
    assert(outer_voice_guard_register(falling, pair, 2, false, 12, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[1].present);
}

static void test_gap_bridging_marks_short_rests_only(void) {
    OuterNote notes[3] = {note16("a", 0, 4, 60), note16("b", 8, 12, 62), note16("c", 20, 24, 64)};
    size_t a[1] = {0};
    size_t b[1] = {1};
    size_t c[1] = {2};
    OuterSegment segments[6] = {beat_segment(0, a, 1), beat_segment(1, NULL, 0), beat_segment(2, b, 1),
                                beat_segment(3, NULL, 0), beat_segment(4, NULL, 0), beat_segment(5, c, 1)};
    /* beat 0 note a, beat 1 rest, beat 2 note b (a one-beat... the gap from a's end at 4/16 to beat 2 at 8/16
       is 4/16: exactly one beat, not under it), beats 3-4 rest, beat 5 note c. */
    OuterNode nodes[6];
    outer_voice_pick_line(notes, segments, 6, true, nodes);
    assert(outer_voice_mark_bridged_gaps(notes, segments, 6, rational_ratio(2, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(nodes[1].bridged);                       /* 4/16 of a gap is under two beats */
    assert(!nodes[3].bridged && !nodes[4].bridged); /* b ends at 12/16, c starts at 20/16: two beats, not under */

    /* With the limit at one beat, the first gap (exactly a beat) is no longer short. */
    outer_voice_pick_line(notes, segments, 6, true, nodes);
    assert(outer_voice_mark_bridged_gaps(notes, segments, 6, rational_ratio(1, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(!nodes[1].bridged && !nodes[3].bridged);
}

static void test_rests_at_the_ends_of_a_line_are_not_bridged(void) {
    OuterNote notes[1] = {note16("a", 4, 8, 60)};
    size_t a[1] = {0};
    OuterSegment segments[3] = {beat_segment(0, NULL, 0), beat_segment(1, a, 1), beat_segment(2, NULL, 0)};
    OuterNode nodes[3];
    outer_voice_pick_line(notes, segments, 3, true, nodes);
    assert(outer_voice_mark_bridged_gaps(notes, segments, 3, rational_ratio(4, 1), constant_beat, NULL, nodes) ==
           OUTER_VOICE_OK);
    assert(!nodes[0].bridged && !nodes[2].bridged);
}

/* --- the rules through outer_voice_extract --- */

static void test_a_resting_melody_stays_absent_while_the_accompaniment_continues(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "bass", 0, 1, 3, 4, 36);   /* C2 held under beats 1 to 3 */
    add_note_in(&set, tuning, "chord", 0, 1, 3, 4, 64);  /* E4 */
    add_note_in(&set, tuning, "m0", 0, 1, 1, 4, 86);     /* melody D6 in beat 1 */
    add_note_in(&set, tuning, "m2", 1, 2, 1, 4, 89);     /* melody F6 in beat 3; beat 2 rests */
    MeterMap *map = four_four();
    /* A rest of a whole beat is not "under one beat", so the limit is two beats here. */
    OuterVoiceConfig config = outer_voice_config_default();
    config.gap_bridge_max_numerator = 2;
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&set, map, &config, &segments, &lines);

    assert(lines.count == 3);
    assert_entry(&lines.soprano[0], "m0", 86);
    assert(!lines.soprano[1].present && lines.soprano[1].guarded && lines.soprano[1].bridged);
    assert(lines.soprano[1].note_id == NULL && lines.soprano[1].pitch == NULL);
    assert_span(&lines.soprano[1], 1, 4, 1, 2); /* the absent entry keeps its segment's span */
    assert_entry(&lines.soprano[2], "m2", 89);
    for (size_t i = 0; i < 3; i++) {
        assert_entry(&lines.bass[i], "bass", 36); /* the bass line is unbroken */
    }
    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);

    /* With the guard off, the soprano adopts the accompaniment's top note in the rest. */
    config.guard_soprano_register = false;
    config.bridge_gaps = false;
    extract(&set, map, &config, &segments, &lines);
    assert_entry(&lines.soprano[1], "chord", 64);
    assert(!lines.soprano[1].guarded && !lines.soprano[1].bridged);
    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);

    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_a_bass_rest_is_bridged(void) {
    /* Bass C2 in beat 1, resting in beat 2 while upper voices continue, back in beat 3. */
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, 0};
    add_note_in(&set, tuning, "b0", 0, 1, 1, 4, 36);
    add_note_in(&set, tuning, "b2", 1, 2, 1, 4, 38);
    add_note_in(&set, tuning, "up", 0, 1, 3, 4, 72);
    add_note_in(&set, tuning, "mid", 0, 1, 3, 4, 67);
    MeterMap *map = four_four();
    OuterVoiceConfig config = outer_voice_config_default();
    config.gap_bridge_max_numerator = 2; /* a rest of a whole beat is not under one beat */
    AdaptiveSonorities segments = {0};
    OuterVoiceLines lines = {0};
    extract(&set, map, &config, &segments, &lines);

    assert(lines.count == 3);
    assert_entry(&lines.bass[0], "b0", 36);
    assert(!lines.bass[1].present && lines.bass[1].guarded && lines.bass[1].bridged);
    assert_entry(&lines.bass[2], "b2", 38);
    for (size_t i = 0; i < 3; i++) {
        assert_entry(&lines.soprano[i], "up", 72);
    }
    outer_voice_lines_destroy(&lines);
    adaptive_sonorities_destroy(&segments);
    meter_map_destroy(map);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

int main(void) {
    test_config_defaults();
    test_a_single_voice_is_both_outer_lines();
    test_a_melody_over_block_chords();
    test_a_doubled_top_note_picks_the_most_recent_attack_then_the_lowest_id();
    test_an_alberti_bass_has_the_lowest_note_of_each_segment_as_its_bass();
    test_silence_between_segments_makes_no_entry();
    test_pick_line_directly();
    test_rejects_bad_input();
    test_the_soprano_guard_keeps_a_resting_melody_absent();
    test_the_soprano_guard_does_not_fire_without_a_big_enough_drop_or_a_short_enough_gap();
    test_the_soprano_stays_absent_until_the_window_passes();
    test_the_bass_guard_mirrors_the_soprano_guard();
    test_gap_bridging_marks_short_rests_only();
    test_rests_at_the_ends_of_a_line_are_not_bridged();
    test_a_resting_melody_stays_absent_while_the_accompaniment_continues();
    test_a_bass_rest_is_bridged();
    printf("test_outer_voice: ok\n");
    return 0;
}
