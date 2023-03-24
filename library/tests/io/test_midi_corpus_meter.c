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

typedef struct {
    unsigned top;
    unsigned bottom;
} Signature;

/* The meter map of a corpus file: its regions' signatures in order, the number of measures the notes
   reach (meter_map_measure_count at the last note-off), and the measure of the final note, the last
   note by start time. */
static void check_piece(const char *name, const Signature *signatures, size_t region_count,
                        size_t expected_measure_count, int expected_final_note_measure) {
    Imported imported = import_corpus_file(name);
    const MeterMap *map = imported.meter.meter_map;

    assert(meter_map_region_count(map) == region_count);
    for (size_t i = 0; i < region_count; i++) {
        assert_signature(map, i, signatures[i].top, signatures[i].bottom);
    }
    assert(imported.meter.conflict_count == 0);

    size_t measure_count = 0;
    assert(meter_map_measure_count(map, imported.meter.end_time, &measure_count) == METER_MAP_OK);
    assert(measure_count == expected_measure_count);
    assert(measure_of_note(&imported, imported.count - 1) == expected_final_note_measure);

    release(&imported);
}

/*
 * Bach, Minuet in G, BWV Anh. 114 (Notebook for Anna Magdalena Bach).
 *   Score: 3/4, 32 measures (two 16-measure strains), so the final note is in measure 32. This is the
 *   published score's meter and length; it is not re-checked against an edition in this repository.
 *   File: declares 4/4 at tick 0 (0xFF 0x58: 4 2 24 8) at 384 ticks per quarter; the notes end at tick
 *   36864, which is 96 quarter notes.
 * DISCREPANCY: the file's meter is wrong for the piece. 96 quarter notes is 32 measures of 3/4 but 24
 * of 4/4, so measure numbers read from this file run 24 where the score runs 32. This test asserts
 * what the file encodes (4/4, 24 measures, final note in measure 24); the importer does not infer a
 * meter from note content to correct it.
 */
static void test_bach_114(void) {
    const Signature signatures[] = {{4, 4}};
    check_piece("Bach 114.mid", signatures, 1, 24, 24);
}

/*
 * Mozart, Sonata K. 545 (the file holds the whole sonata, three movements).
 *   Score: the first movement is in common time (4/4); the slow movement is 3/4 and the finale 2/4.
 *   Source: the score (K. 545); the per-movement measure counts and repeats are not re-checked here.
 *   File: 240 ticks per quarter, four meter events in track 0: 4/4 at tick 0, 3/4 at 142080, 1/4 at
 *   219120 (one quarter note long) and 2/4 at 219360.
 * DISCREPANCY: the score numbers each movement from 1, but a MIDI file is one timeline, so the file
 * numbers straight through. The 4/4 region is 148 measures, the 3/4 region is
 * 107 (measures 149-255), the one-quarter region is measure 256, and the 2/4 region runs from 257 to
 * 338. The one-quarter region is the finale's upbeat, encoded as its own short signature in the
 * middle of the file; it is not the first region, so the pickup rule does not apply and it takes a
 * measure number. This test asserts what the file encodes: score measure counts are NOT reproduced
 * (file-derived, score unverified).
 */
static void test_mozart_545(void) {
    const Signature signatures[] = {{4, 4}, {3, 4}, {1, 4}, {2, 4}};
    check_piece("Mozart 545.mid", signatures, 4, 338, 338);

    Imported imported = import_corpus_file("Mozart 545.mid");
    const MeterMap *map = imported.meter.meter_map;
    assert(!meter_map_has_pickup(map));
    assert(meter_map_region_first_measure_number(meter_map_region(map, 0)) == 1);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 1)) == 149);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 2)) == 256);
    assert(meter_map_region_first_measure_number(meter_map_region(map, 3)) == 257);
    release(&imported);
}

/*
 * Clementi, a Sonatina from Op. 36 (the README does not say which sonatina or movement).
 *   Score: 4/4 expected from the file; the score's meter and measure count are not re-checked
 *   here (file-derived, score unverified).
 *   File: declares 4/4 at tick 0 (0xFF 0x58: 4 2 24 8) at 240 ticks per quarter. Notes run to tick
 *   36230, 37.7 measures of 960 ticks, so the 38th measure is a partial one: counted, as the final
 *   note starts at tick 36000 inside it.
 */
static void test_clementi_36(void) {
    const Signature signatures[] = {{4, 4}};
    check_piece("Clementi 36.mid", signatures, 1, 38, 38);
}

/*
 * Field, Nocturne No. 1 in E-flat.
 *   Score: 12/8 (stated in the task brief for this import). The score's measure count is not
 *   re-checked here (file-derived, score unverified).
 *   File: 12/8 at tick 0 (0xFF 0x58: 12 3 12 8), 240 ticks per quarter, so a measure is 1440
 *   ticks. The first note starts at tick 1080, nine eighth notes into measure 1, and the notes end at
 *   tick 96120, 66.75 measures in: 67 measures, the final note in measure 67.
 * POSSIBLE DISCREPANCY: a first note nine eighths into a 12/8 measure is the shape of a padded upbeat
 * (an upbeat of three eighths written after nine eighths of rest). The file says nothing about a
 * pickup (no short leading signature, no partial first measure), so the importer numbers that
 * measure 1 and this test asserts the file's numbering. If the score counts that upbeat as measure
 * 0, its numbers run one lower than the file's; no pickup is inferred from note content to match.
 */
static void test_field_1(void) {
    const Signature signatures[] = {{12, 8}};
    check_piece("Field 1.mid", signatures, 1, 67, 67);
}

/*
 * Field, Nocturne No. 5 in B-flat.
 *   Score: the score's meter and measure count are not re-checked here (file-derived, score
 *   unverified).
 *   File: 12/8 at tick 0 (0xFF 0x58: 12 3 36 8) at 256 ticks per quarter, so a measure is 1536 ticks.
 *   The notes end at tick 65792, 42.8 measures in: 43 measures, the final note in measure 43.
 */
static void test_field_5(void) {
    const Signature signatures[] = {{12, 8}};
    check_piece("Field 5.mid", signatures, 1, 43, 43);
}

int main(void) {
    test_field_1_imports_as_twelve_eight();
    test_bach_114();
    test_mozart_545();
    test_clementi_36();
    test_field_1();
    test_field_5();
    printf("test_midi_corpus_meter: ok\n");
    return 0;
}
