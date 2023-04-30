#include "io/midi_importer.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "events/note_event.h"
#include "time/temporal_anchor.h"

/*
 * Tempo of the framework-v1-corpus files, read through midi_importer_import_with_tempo. The corpus
 * directory is passed in as URSATZ_CORPUS_DIR.
 *
 * Every expected value below is a value FOUND IN THE FILE, not a value from the score: the tempos
 * are whatever the MIDI encodes (these look like an export of a performance or a sequencer's tempo
 * curve, not the composer's marking), and the region counts are the number of distinct ticks that
 * carry a set-tempo event. Where a piece has many regions that is the file's own tempo curve; this
 * test does not say it matches any tempo in the printed score.
 *
 * The seconds at the end of each piece were worked out independently of this library, as an exact
 * sum of (whole notes in each region) * 4 * T / 1,000,000 with Python fractions, and are compared
 * exactly.
 */

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    MidiTempoImport tempo;
    MidiMeterImport meter; /* only for the end time of the last note */
} Imported;

static Imported import_corpus_file(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, name);
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length);
    assert(data != NULL);
    size_t read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    assert(read_count == (size_t)length);

    Imported imported = {NULL, NULL, 0, {NULL, NULL, 0}, {NULL, NULL, NULL, 0}};
    assert(midi_importer_import_with_tempo(data, (size_t)length, &imported.events, &imported.pitch_values,
                                           &imported.count, &imported.tempo) == MIDI_IMPORT_OK);

    /* The end time of the last note comes from the meter import; the notes it returns are the same. */
    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    assert(midi_importer_import_with_meter(data, (size_t)length, &events, &pitch_values, &count,
                                           &imported.meter) == MIDI_IMPORT_OK);
    assert(count == imported.count);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
    free(data);
    assert(imported.count > 0);
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
    midi_meter_import_destroy(&imported->meter);
}

static void check_piece(const char *name, unsigned long first_tempo, size_t region_count, long end_seconds_n,
                        unsigned long end_seconds_d) {
    Imported imported = import_corpus_file(name);
    const TempoMap *map = imported.tempo.tempo_map;

    /* Each file sets a tempo at tick 0, so none of them is defaulted. */
    assert(!tempo_map_is_defaulted(map));
    assert(!tempo_map_region_is_defaulted(tempo_map_region(map, 0)));
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(map, 0)) == first_tempo);
    assert(tempo_map_region_count(map) == region_count);
    assert(imported.tempo.conflict_count == 0);

    /* Exact seconds to the last note-off, and back. */
    PerformanceTime *end = NULL;
    assert(tempo_map_seconds_at(map, imported.meter.end_time, &end) == TEMPO_MAP_OK);
    PerformanceTime *expected = NULL;
    assert(performance_time_create(end_seconds_n, end_seconds_d, &expected) == PERFORMANCE_TIME_OK);
    assert(performance_time_equals(end, expected));
    MusicalTime *back = NULL;
    assert(tempo_map_time_at_seconds(map, end, &back) == TEMPO_MAP_OK);
    assert(musical_time_equals(back, imported.meter.end_time));
    musical_time_destroy(back);
    performance_time_destroy(expected);
    performance_time_destroy(end);

    /* Every note's start survives the round trip through seconds. */
    for (size_t i = 0; i < imported.count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(imported.events[i]));
        PerformanceTime *seconds = NULL;
        assert(tempo_map_seconds_at(map, position, &seconds) == TEMPO_MAP_OK);
        MusicalTime *again = NULL;
        assert(tempo_map_time_at_seconds(map, seconds, &again) == TEMPO_MAP_OK);
        assert(musical_time_equals(again, position));
        musical_time_destroy(again);
        performance_time_destroy(seconds);
    }

    release(&imported);
}

/* File values: one tempo, 857142 microseconds per quarter note (70 per minute). */
static void test_bach_114(void) {
    check_piece("Bach 114.mid", 857142, 1, 1285713, 15625);
}

/* File values: 24 tempo events from 454545 (132 per minute); one repeats its neighbour's value and is
   still its own region. */
static void test_mozart_545(void) {
    check_piece("Mozart 545.mid", 454545, 24, 1652768771L, 2400000);
}

/* File values: one tempo, 340909 microseconds per quarter note (176 per minute). */
static void test_clementi_36(void) {
    check_piece("Clementi 36.mid", 340909, 1, 1235113307L, 24000000);
}

/* File values: 602 tempo events from 666667 (90 per minute), a tempo curve. */
static void test_field_1(void) {
    check_piece("Field 1.mid", 666667, 602, 744416659L, 3000000);
}

/* File values: 89 tempo events from 666665 (90 per minute). */
static void test_field_5(void) {
    check_piece("Field 5.mid", 666665, 89, 22551355969L, 128000000);
}

int main(void) {
    test_bach_114();
    test_mozart_545();
    test_clementi_36();
    test_field_1();
    test_field_5();
    printf("test_midi_corpus_tempo: ok\n");
    return 0;
}
