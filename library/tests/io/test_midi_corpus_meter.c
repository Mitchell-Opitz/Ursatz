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
 * Meter of the framework-v1-corpus files, read through midi_importer_import_with_meter. The corpus
 * directory is passed in as URSATZ_CORPUS_DIR.
 */

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    MidiMeterImport meter;
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

    Imported imported = {NULL, NULL, 0, {NULL, NULL, NULL, 0}};
    assert(midi_importer_import_with_meter(data, (size_t)length, &imported.events, &imported.pitch_values,
                                           &imported.count, &imported.meter) == MIDI_IMPORT_OK);
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
    midi_meter_import_destroy(&imported->meter);
}

static int measure_of_note(const Imported *imported, size_t note) {
    const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(imported->events[note]));
    int index = -99;
    assert(meter_map_measure_index_at(imported->meter.meter_map, position, &index) == METER_MAP_OK);
    return index;
}

static void assert_signature(const MeterMap *map, size_t region, unsigned top, unsigned bottom) {
    const MeterMapRegion *r = meter_map_region(map, region);
    assert(r != NULL);
    assert(time_signature_top_number(meter_map_region_time_signature(r)) == top);
    assert(time_signature_bottom_number(meter_map_region_time_signature(r)) == bottom);
    assert(!meter_map_region_is_defaulted(r));
}

/*
 * Field, Nocturne No. 1 in E-flat: written in 12/8 (source: the score; the task brief for this
 * import also states it). The file declares 12/8 at tick 0 (0xFF 0x58: 12 3 12 8), so the import is
 * a declared 12/8, not a default. The first note starts at tick 1080 of 240 per quarter, nine
 * eighth notes into the first 12/8 measure: the file has no pickup region, and this test asserts
 * only what the file encodes.
 */
static void test_field_1_imports_as_twelve_eight(void) {
    Imported imported = import_corpus_file("Field 1.mid");
    const MeterMap *map = imported.meter.meter_map;

    assert(!meter_map_is_defaulted(map));
    assert(meter_map_region_count(map) == 1);
    assert_signature(map, 0, 12, 8);
    assert(!meter_map_has_pickup(map));
    assert(meter_map_first_measure_number(map) == 1);
    assert(imported.meter.conflict_count == 0);
    assert(measure_of_note(&imported, 0) == 1);

    release(&imported);
}

int main(void) {
    test_field_1_imports_as_twelve_eight();
    printf("test_midi_corpus_meter: ok\n");
    return 0;
}
