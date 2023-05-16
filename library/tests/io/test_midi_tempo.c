#include "io/midi_importer.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events/note_event.h"
#include "time/temporal_anchor.h"

/*
 * Tempo import fixtures: SMF files built in code with a constant tempo, a step series that speeds up
 * (an accelerando in steps), a tempo on a non-first track, duplicate and conflicting tempos across
 * tracks, and malformed set-tempo events. Each is imported with midi_importer_import_with_tempo and
 * checked for its regions and for the seconds a chosen note starts at. Division is 480 ticks per
 * quarter note, so a whole note is 1920 ticks.
 */

#define DIVISION 480
#define WHOLE (4 * DIVISION)

typedef struct {
    unsigned char bytes[2048];
    size_t size;
    uint32_t tick;
} TrackBuilder;

static void put(TrackBuilder *t, unsigned char byte) {
    assert(t->size < sizeof(t->bytes));
    t->bytes[t->size++] = byte;
}

static void put_delta(TrackBuilder *t, uint32_t delta) {
    unsigned char groups[5];
    size_t count = 0;
    groups[count++] = (unsigned char)(delta & 0x7F);
    while ((delta >>= 7) > 0) {
        groups[count++] = (unsigned char)((delta & 0x7F) | 0x80);
    }
    while (count > 0) {
        put(t, groups[--count]);
    }
}

/* Events must be added in non-decreasing tick order within a track. */
static void add_tempo(TrackBuilder *t, uint32_t at, uint32_t microseconds_per_quarter) {
    assert(at >= t->tick);
    put_delta(t, at - t->tick);
    t->tick = at;
    put(t, 0xFF);
    put(t, 0x51);
    put(t, 0x03);
    put(t, (unsigned char)(microseconds_per_quarter >> 16));
    put(t, (unsigned char)(microseconds_per_quarter >> 8));
    put(t, (unsigned char)microseconds_per_quarter);
}

/* A set-tempo meta-event with a payload of the caller's choosing, for malformed cases. */
static void add_raw_tempo(TrackBuilder *t, uint32_t at, const unsigned char *payload, unsigned char length) {
    assert(at >= t->tick);
    put_delta(t, at - t->tick);
    t->tick = at;
    put(t, 0xFF);
    put(t, 0x51);
    put(t, length);
    for (unsigned char i = 0; i < length; i++) {
        put(t, payload[i]);
    }
}

static void add_note(TrackBuilder *t, uint32_t at, uint32_t duration, unsigned char key) {
    assert(at >= t->tick);
    put_delta(t, at - t->tick);
    put(t, 0x90);
    put(t, key);
    put(t, 0x64);
    put_delta(t, duration);
    put(t, 0x80);
    put(t, key);
    put(t, 0x40);
    t->tick = at + duration;
}

static size_t build_smf(unsigned char *out, uint16_t format, const TrackBuilder *tracks, uint16_t track_count) {
    size_t pos = 0;
    const unsigned char header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, (unsigned char)format,
                                    (unsigned char)(track_count >> 8), (unsigned char)track_count,
                                    (unsigned char)(DIVISION >> 8), (unsigned char)(DIVISION & 0xFF)};
    memcpy(out + pos, header, sizeof(header));
    pos += sizeof(header);
    for (uint16_t i = 0; i < track_count; i++) {
        memcpy(out + pos, "MTrk", 4);
        pos += 4;
        out[pos++] = (unsigned char)(tracks[i].size >> 24);
        out[pos++] = (unsigned char)(tracks[i].size >> 16);
        out[pos++] = (unsigned char)(tracks[i].size >> 8);
        out[pos++] = (unsigned char)tracks[i].size;
        memcpy(out + pos, tracks[i].bytes, tracks[i].size);
        pos += tracks[i].size;
    }
    return pos;
}

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    MidiTempoImport tempo;
} Imported;

static Imported import_tracks(uint16_t format, const TrackBuilder *tracks, uint16_t track_count) {
    static unsigned char file[8192];
    size_t size = build_smf(file, format, tracks, track_count);
    Imported imported = {NULL, NULL, 0, {NULL, NULL, 0}};
    assert(midi_importer_import_with_tempo(file, size, &imported.events, &imported.pitch_values, &imported.count,
                                           &imported.tempo) == MIDI_IMPORT_OK);
    return imported;
}

static void release(Imported *imported) {
    for (size_t i = 0; i < imported->count; i++) {
        note_event_destroy(imported->events[i]);
        pitch_numeric_value_destroy(imported->pitch_values[i]);
    }
    free(imported->events);
    free(imported->pitch_values);
    midi_tempo_import_destroy(&imported->tempo);
}

static void assert_note_seconds(const Imported *imported, size_t note, long sn, unsigned long sd) {
    const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(imported->events[note]));
    PerformanceTime *seconds = NULL;
    assert(tempo_map_seconds_at(imported->tempo.tempo_map, position, &seconds) == TEMPO_MAP_OK);
    PerformanceTime *expected = NULL;
    assert(performance_time_create(sn, sd, &expected) == PERFORMANCE_TIME_OK);
    assert(performance_time_equals(seconds, expected));
    performance_time_destroy(expected);
    performance_time_destroy(seconds);
}

static unsigned long region_tempo(const Imported *imported, size_t region) {
    const TempoMapRegion *r = tempo_map_region(imported->tempo.tempo_map, region);
    assert(r != NULL);
    return tempo_map_region_microseconds_per_quarter(r);
}

static void test_constant_tempo(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_tempo(&track, 0, 500000);
    for (uint32_t k = 0; k < 4; k++) {
        add_note(&track, k * WHOLE, DIVISION, 60);
    }
    Imported imported = import_tracks(0, &track, 1);

    assert(!tempo_map_is_defaulted(imported.tempo.tempo_map));
    assert(tempo_map_region_count(imported.tempo.tempo_map) == 1);
    assert(region_tempo(&imported, 0) == 500000);
    assert_note_seconds(&imported, 0, 0, 1);
    assert_note_seconds(&imported, 1, 2, 1); /* a whole note at 120 quarter notes per minute */
    assert_note_seconds(&imported, 3, 6, 1);

    release(&imported);
}

static void test_step_series_that_speeds_up(void) {
    /* A whole note per step, each faster: 600,000, 500,000, 400,000, 300,000. */
    TrackBuilder track = {{0}, 0, 0};
    add_tempo(&track, 0, 600000);
    add_note(&track, 0, DIVISION, 60);
    add_tempo(&track, 1 * WHOLE, 500000);
    add_note(&track, 1 * WHOLE, DIVISION, 62);
    add_tempo(&track, 2 * WHOLE, 400000);
    add_note(&track, 2 * WHOLE, DIVISION, 64);
    add_tempo(&track, 3 * WHOLE, 300000);
    add_note(&track, 3 * WHOLE, DIVISION, 65);
    add_note(&track, 4 * WHOLE, DIVISION, 67);
    Imported imported = import_tracks(0, &track, 1);

    assert(tempo_map_region_count(imported.tempo.tempo_map) == 4);
    assert(region_tempo(&imported, 0) == 600000);
    assert(region_tempo(&imported, 1) == 500000);
    assert(region_tempo(&imported, 2) == 400000);
    assert(region_tempo(&imported, 3) == 300000);

    /* 2.4 s, then 2.0, 1.6 and 1.2 s a whole note: 0, 12/5, 22/5, 6, 36/5. */
    assert_note_seconds(&imported, 0, 0, 1);
    assert_note_seconds(&imported, 1, 12, 5);
    assert_note_seconds(&imported, 2, 22, 5);
    assert_note_seconds(&imported, 3, 6, 1);
    assert_note_seconds(&imported, 4, 36, 5);

    release(&imported);
}

static void test_tempo_on_a_non_first_track(void) {
    TrackBuilder tracks[2] = {{{0}, 0, 0}, {{0}, 0, 0}};
    add_note(&tracks[0], 0, DIVISION, 60);
    add_note(&tracks[0], WHOLE, DIVISION, 62);
    add_note(&tracks[0], 2 * WHOLE, DIVISION, 64);
    add_tempo(&tracks[1], WHOLE, 1000000);
    Imported imported = import_tracks(1, tracks, 2);

    /* No tempo at tick 0, so the default covers the first whole note. */
    assert(tempo_map_region_count(imported.tempo.tempo_map) == 2);
    assert(tempo_map_region_is_defaulted(tempo_map_region(imported.tempo.tempo_map, 0)));
    assert(region_tempo(&imported, 0) == 500000);
    assert(region_tempo(&imported, 1) == 1000000);
    assert(!tempo_map_is_defaulted(imported.tempo.tempo_map));
    assert_note_seconds(&imported, 1, 2, 1);
    assert_note_seconds(&imported, 2, 6, 1);

    release(&imported);
}

static void test_duplicates_across_tracks_merge(void) {
    TrackBuilder tracks[2] = {{{0}, 0, 0}, {{0}, 0, 0}};
    add_tempo(&tracks[0], 0, 500000);
    add_note(&tracks[0], 0, DIVISION, 60);
    add_tempo(&tracks[1], 0, 500000);
    Imported imported = import_tracks(1, tracks, 2);

    assert(tempo_map_region_count(imported.tempo.tempo_map) == 1);
    assert(imported.tempo.conflict_count == 0);
    assert(region_tempo(&imported, 0) == 500000);

    release(&imported);
}

static void test_conflicting_tempos_across_tracks_last_track_wins(void) {
    TrackBuilder tracks[2] = {{{0}, 0, 0}, {{0}, 0, 0}};
    add_tempo(&tracks[0], 0, 500000);
    add_note(&tracks[0], 0, DIVISION, 60);
    add_tempo(&tracks[1], 0, 800000);
    Imported imported = import_tracks(1, tracks, 2);

    assert(tempo_map_region_count(imported.tempo.tempo_map) == 1);
    assert(imported.tempo.conflict_count == 1);
    assert(imported.tempo.conflict_ticks[0] == 0);
    assert(region_tempo(&imported, 0) == 800000);

    release(&imported);
}

static void test_malformed_set_tempo_events_are_skipped(void) {
    const unsigned char too_short[] = {0x07, 0xA1};
    const unsigned char too_long[] = {0x00, 0x07, 0xA1, 0x20};
    const unsigned char zero[] = {0x00, 0x00, 0x00};
    TrackBuilder track = {{0}, 0, 0};
    add_raw_tempo(&track, 0, too_short, sizeof(too_short));
    add_raw_tempo(&track, 0, too_long, sizeof(too_long));
    add_raw_tempo(&track, 0, zero, sizeof(zero));
    add_note(&track, 0, DIVISION, 60);
    Imported imported = import_tracks(0, &track, 1);

    /* Nothing valid was declared: the default applies, marked as such. */
    assert(tempo_map_is_defaulted(imported.tempo.tempo_map));
    assert(tempo_map_region_count(imported.tempo.tempo_map) == 1);
    assert(region_tempo(&imported, 0) == 500000);
    assert(imported.count == 1);
    release(&imported);

    /* A good event after the bad ones is read. */
    TrackBuilder mixed = {{0}, 0, 0};
    add_raw_tempo(&mixed, 0, zero, sizeof(zero));
    add_tempo(&mixed, 0, 750000);
    add_note(&mixed, 0, DIVISION, 60);
    imported = import_tracks(0, &mixed, 1);
    assert(!tempo_map_is_defaulted(imported.tempo.tempo_map));
    assert(region_tempo(&imported, 0) == 750000);
    release(&imported);
}

static void test_no_set_tempo_is_a_defaulted_120(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_note(&track, 0, DIVISION, 60);
    add_note(&track, WHOLE, DIVISION, 62);
    Imported imported = import_tracks(0, &track, 1);

    assert(tempo_map_is_defaulted(imported.tempo.tempo_map));
    assert(region_tempo(&imported, 0) == TEMPO_MAP_DEFAULT_MICROSECONDS_PER_QUARTER);
    assert_note_seconds(&imported, 1, 2, 1);

    release(&imported);
}

int main(void) {
    test_constant_tempo();
    test_step_series_that_speeds_up();
    test_tempo_on_a_non_first_track();
    test_duplicates_across_tracks_merge();
    test_conflicting_tempos_across_tracks_last_track_wins();
    test_malformed_set_tempo_events_are_skipped();
    test_no_set_tempo_is_a_defaulted_120();
    printf("test_midi_tempo: ok\n");
    return 0;
}
