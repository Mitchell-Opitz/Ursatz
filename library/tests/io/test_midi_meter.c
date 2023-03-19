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
 * Meter import fixtures: SMF files built in code in 3/4, 6/8 and 12/8, a 4/4 to 3/4 meter change at
 * measure 5, and a 1/8 pickup before 12/8. Each is imported with midi_importer_import_with_meter
 * and checked for the measure index of chosen notes and for the span of chosen measures. Division
 * is 480 ticks per quarter note, so a whole note is 1920 ticks and an eighth note is 240.
 */

#define DIVISION 480

typedef struct {
    unsigned char bytes[4096];
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

/* Events must be added in non-decreasing tick order. */
static void add_time_signature(TrackBuilder *t, uint32_t at, unsigned nn, unsigned dd_exponent) {
    assert(at >= t->tick);
    put_delta(t, at - t->tick);
    t->tick = at;
    put(t, 0xFF);
    put(t, 0x58);
    put(t, 0x04);
    put(t, (unsigned char)nn);
    put(t, (unsigned char)dd_exponent);
    put(t, 0x18);
    put(t, 0x08);
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

static size_t build_smf(unsigned char *out, const TrackBuilder *t) {
    size_t pos = 0;
    const unsigned char header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1,
                                    (unsigned char)(DIVISION >> 8), (unsigned char)(DIVISION & 0xFF)};
    memcpy(out + pos, header, sizeof(header));
    pos += sizeof(header);
    memcpy(out + pos, "MTrk", 4);
    pos += 4;
    out[pos++] = (unsigned char)(t->size >> 24);
    out[pos++] = (unsigned char)(t->size >> 16);
    out[pos++] = (unsigned char)(t->size >> 8);
    out[pos++] = (unsigned char)t->size;
    memcpy(out + pos, t->bytes, t->size);
    return pos + t->size;
}

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    MidiMeterImport meter;
} Imported;

static Imported import_track(const TrackBuilder *track) {
    static unsigned char file[8192];
    size_t size = build_smf(file, track);
    Imported imported = {NULL, NULL, 0, {NULL, NULL, NULL, 0}};
    assert(midi_importer_import_with_meter(file, size, &imported.events, &imported.pitch_values, &imported.count,
                                           &imported.meter) == MIDI_IMPORT_OK);
    return imported;
}

static void release(Imported *imported) {
    for (size_t i = 0; i < imported->count; i++) {
        note_event_destroy(imported->events[i]);
        pitch_numeric_value_destroy(imported->pitch_values[i]);
    }
    free(imported->events);
    free(imported->pitch_values);
    midi_meter_import_destroy(&imported->meter);
}

static int measure_of_note(const Imported *imported, size_t note) {
    const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(imported->events[note]));
    int index = -99;
    assert(meter_map_measure_index_at(imported->meter.meter_map, position, &index) == METER_MAP_OK);
    return index;
}

static void assert_measure_span(const Imported *imported, int index, long sn, unsigned long sd, long en,
                                unsigned long ed) {
    TimeSpan *span = NULL;
    assert(meter_map_measure_span(imported->meter.meter_map, index, &span) == METER_MAP_OK);
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    musical_time_create(sn, sd, &start);
    musical_time_create(en, ed, &end);
    assert(musical_time_equals(time_span_start(span), start));
    assert(musical_time_equals(time_span_end(span), end));
    musical_time_destroy(start);
    musical_time_destroy(end);
    time_span_destroy(span);
}

static size_t measure_count_of(const Imported *imported) {
    size_t count = 0;
    assert(meter_map_measure_count(imported->meter.meter_map, imported->meter.end_time, &count) == METER_MAP_OK);
    return count;
}

static void assert_signature(const Imported *imported, size_t region, unsigned top, unsigned bottom) {
    const MeterMapRegion *r = meter_map_region(imported->meter.meter_map, region);
    assert(r != NULL);
    assert(time_signature_top_number(meter_map_region_time_signature(r)) == top);
    assert(time_signature_bottom_number(meter_map_region_time_signature(r)) == bottom);
}

static void test_three_four(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_time_signature(&track, 0, 3, 2);
    for (uint32_t k = 0; k < 8; k++) {
        add_note(&track, k * DIVISION, DIVISION, 60);
    }
    Imported imported = import_track(&track);

    assert_signature(&imported, 0, 3, 4);
    assert(!meter_map_is_defaulted(imported.meter.meter_map));
    assert(imported.count == 8);
    assert(measure_of_note(&imported, 0) == 1);
    assert(measure_of_note(&imported, 2) == 1);
    assert(measure_of_note(&imported, 3) == 2); /* the barline note belongs to measure 2 */
    assert(measure_of_note(&imported, 5) == 2);
    assert(measure_of_note(&imported, 6) == 3);
    assert(measure_of_note(&imported, 7) == 3);
    assert_measure_span(&imported, 1, 0, 1, 3, 4);
    assert_measure_span(&imported, 2, 3, 4, 3, 2);
    assert(measure_count_of(&imported) == 3); /* 8 quarter notes is 2 measures and two thirds */

    release(&imported);
}

static void test_six_eight(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_time_signature(&track, 0, 6, 3);
    for (uint32_t k = 0; k < 12; k++) {
        add_note(&track, k * (DIVISION / 2), DIVISION / 2, 62);
    }
    Imported imported = import_track(&track);

    assert_signature(&imported, 0, 6, 8);
    assert(measure_of_note(&imported, 0) == 1);
    assert(measure_of_note(&imported, 5) == 1);
    assert(measure_of_note(&imported, 6) == 2);
    assert(measure_of_note(&imported, 11) == 2);
    assert_measure_span(&imported, 1, 0, 1, 3, 4);
    assert_measure_span(&imported, 2, 3, 4, 3, 2);
    assert(measure_count_of(&imported) == 2); /* ends exactly on the barline: no empty third measure */

    release(&imported);
}

static void test_twelve_eight(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_time_signature(&track, 0, 12, 3);
    for (uint32_t k = 0; k < 14; k++) {
        add_note(&track, k * (DIVISION / 2), DIVISION / 2, 64);
    }
    Imported imported = import_track(&track);

    assert_signature(&imported, 0, 12, 8);
    assert(measure_of_note(&imported, 0) == 1);
    assert(measure_of_note(&imported, 11) == 1);
    assert(measure_of_note(&imported, 12) == 2);
    assert(measure_of_note(&imported, 13) == 2);
    assert_measure_span(&imported, 1, 0, 1, 3, 2);
    assert_measure_span(&imported, 2, 3, 2, 3, 1);
    assert(measure_count_of(&imported) == 2);

    release(&imported);
}

static void test_meter_change_at_measure_five(void) {
    /* 4/4 for four measures (7680 ticks), then 3/4. */
    TrackBuilder track = {{0}, 0, 0};
    add_time_signature(&track, 0, 4, 2);
    add_note(&track, 0, DIVISION, 60);
    add_note(&track, 7200, DIVISION, 62); /* the last quarter of measure 4 */
    add_time_signature(&track, 7680, 3, 2);
    add_note(&track, 7680, DIVISION, 64);        /* first note of measure 5 */
    add_note(&track, 7680 + 1440, DIVISION, 65); /* first note of measure 6 */
    Imported imported = import_track(&track);

    assert(meter_map_region_count(imported.meter.meter_map) == 2);
    assert_signature(&imported, 0, 4, 4);
    assert_signature(&imported, 1, 3, 4);
    assert(measure_of_note(&imported, 0) == 1);
    assert(measure_of_note(&imported, 1) == 4);
    assert(measure_of_note(&imported, 2) == 5);
    assert(measure_of_note(&imported, 3) == 6);
    assert_measure_span(&imported, 4, 3, 1, 4, 1);
    assert_measure_span(&imported, 5, 4, 1, 19, 4);
    assert_measure_span(&imported, 6, 19, 4, 11, 2);
    assert(measure_count_of(&imported) == 6);

    release(&imported);
}

static void test_one_eighth_pickup_before_twelve_eight(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_time_signature(&track, 0, 1, 3);
    add_note(&track, 0, DIVISION / 2, 67); /* the pickup eighth */
    add_time_signature(&track, DIVISION / 2, 12, 3);
    add_note(&track, DIVISION / 2, DIVISION, 69);
    add_note(&track, DIVISION / 2 + 5760, DIVISION, 71); /* 5760 ticks later: the start of measure 3 */
    Imported imported = import_track(&track);

    assert(meter_map_has_pickup(imported.meter.meter_map));
    assert(measure_of_note(&imported, 0) == 0);
    assert(measure_of_note(&imported, 1) == 1);
    assert(measure_of_note(&imported, 2) == 3);
    assert_measure_span(&imported, 0, 0, 1, 1, 8);
    assert_measure_span(&imported, 1, 1, 8, 13, 8);
    assert(measure_count_of(&imported) == 4); /* the pickup and measures 1 to 3 */

    release(&imported);
}

static void test_no_time_signature_is_a_defaulted_four_four(void) {
    TrackBuilder track = {{0}, 0, 0};
    add_note(&track, 0, DIVISION, 60);
    add_note(&track, 1920, DIVISION, 62);
    Imported imported = import_track(&track);

    assert(meter_map_is_defaulted(imported.meter.meter_map));
    assert_signature(&imported, 0, 4, 4);
    assert(measure_of_note(&imported, 0) == 1);
    assert(measure_of_note(&imported, 1) == 2);

    release(&imported);
}

int main(void) {
    test_three_four();
    test_six_eight();
    test_twelve_eight();
    test_meter_change_at_measure_five();
    test_one_eighth_pickup_before_twelve_eight();
    test_no_time_signature_is_a_defaulted_four_four();
    printf("test_midi_meter: ok\n");
    return 0;
}
