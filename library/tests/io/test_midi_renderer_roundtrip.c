#include "midi_render_test_util.h"

/*
 * Round trips: the reference pieces of the framework-v1-corpus (import -> render -> import) and synthetic pieces in
 * 3/4, 6/8, 12/8, with tempo changes and with a pickup. URSATZ_CORPUS_DIR is passed in by CMake.
 */

static unsigned char *read_file(const char *name, size_t *size) {
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
    assert(fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return data;
}

static void round_trip_reference_piece(const char *name) {
    size_t size = 0;
    unsigned char *data = read_file(name, &size);

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    assert(smf_reader_parse(data, size, &header, &tracks, &track_count) == SMF_READER_OK);
    unsigned division = header.division;
    free(tracks);

    NoteEvent **events = NULL;
    PitchNumericValue **pitches = NULL;
    size_t count = 0;
    MidiMeterImport meter = {NULL, NULL, NULL, 0};
    assert(midi_importer_import_with_meter(data, size, &events, &pitches, &count, &meter) == MIDI_IMPORT_OK);
    assert(count > 0);

    NoteEvent **tempo_events = NULL;
    PitchNumericValue **tempo_pitches = NULL;
    size_t tempo_count = 0;
    MidiTempoImport tempo = {NULL, NULL, 0};
    assert(midi_importer_import_with_tempo(data, size, &tempo_events, &tempo_pitches, &tempo_count, &tempo) ==
           MIDI_IMPORT_OK);
    for (size_t i = 0; i < tempo_count; i++) {
        note_event_destroy(tempo_events[i]);
        pitch_numeric_value_destroy(tempo_pitches[i]);
    }
    free(tempo_events);
    free(tempo_pitches);

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(midi_key_signature_scan_all(data, size, &keys) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);

    test_round_trip(events, pitches, count, meter.meter_map, tempo.tempo_map, keys.events, keys.count, division);

    midi_key_signature_events_destroy(&keys);
    midi_tempo_import_destroy(&tempo);
    midi_meter_import_destroy(&meter);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitches[i]);
    }
    free(events);
    free(pitches);
    free(data);
}

static void test_reference_pieces(void) {
    round_trip_reference_piece("Bach 114.mid");
    round_trip_reference_piece("Clementi 36.mid");
    round_trip_reference_piece("Field 1.mid");
    round_trip_reference_piece("Field 5.mid");
    round_trip_reference_piece("Mozart 545.mid");
}

/* A line of quarter-note-grid notes in one voice from position 0, count notes of the given duration. */
static void add_line(TestNotes *notes, long start_numerator, unsigned long start_denominator, long duration_numerator,
                     unsigned long duration_denominator, int count, long first_pitch, const char *voice) {
    for (int i = 0; i < count; i++) {
        long numerator = start_numerator * (long)duration_denominator + (long)i * duration_numerator * (long)start_denominator;
        unsigned long denominator = start_denominator * duration_denominator;
        test_notes_add(notes, numerator, denominator, duration_numerator, duration_denominator, first_pitch + i, voice);
    }
}

static void test_three_four(void) {
    TestNotes notes;
    test_notes_init(&notes);
    add_line(&notes, 0, 1, 1, 4, 9, 60, "melody");
    add_line(&notes, 0, 1, 3, 4, 3, 48, "bass");
    const TestMeterChange meters[] = {{3, 4, 0, 1}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    test_round_trip(notes.events, notes.pitches, notes.count, meter_map, NULL, NULL, 0, 480);
    meter_map_destroy(meter_map);
    test_notes_free(&notes);
}

static void test_six_eight(void) {
    TestNotes notes;
    test_notes_init(&notes);
    add_line(&notes, 0, 1, 1, 8, 12, 62, "melody");
    const TestMeterChange meters[] = {{6, 8, 0, 1}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    test_round_trip(notes.events, notes.pitches, notes.count, meter_map, NULL, NULL, 0, 480);
    meter_map_destroy(meter_map);
    test_notes_free(&notes);
}

static void test_twelve_eight(void) {
    TestNotes notes;
    test_notes_init(&notes);
    add_line(&notes, 0, 1, 1, 8, 24, 55, "melody");
    const TestMeterChange meters[] = {{12, 8, 0, 1}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    test_round_trip(notes.events, notes.pitches, notes.count, meter_map, NULL, NULL, 0, 480);
    meter_map_destroy(meter_map);
    test_notes_free(&notes);
}

static void test_meter_and_tempo_changes(void) {
    TestNotes notes;
    test_notes_init(&notes);
    add_line(&notes, 0, 1, 1, 4, 12, 60, "melody");
    const TestMeterChange meters[] = {{4, 4, 0, 1}, {3, 4, 1, 1}, {6, 8, 7, 4}};
    const TestTempoChange tempos[] = {{0, 1, 500000}, {1, 1, 400000}, {2, 1, 750000}};
    MeterMap *meter_map = test_meter_map(meters, 3);
    TempoMap *tempo_map = test_tempo_map(tempos, 3);
    MidiKeySignatureEvent keys[1];
    memset(keys, 0, sizeof(keys));
    keys[0].position = test_time(0, 1);
    keys[0].sharps_flats = 2;
    test_round_trip(notes.events, notes.pitches, notes.count, meter_map, tempo_map, keys, 1, 480);
    musical_time_destroy(keys[0].position);
    meter_map_destroy(meter_map);
    tempo_map_destroy(tempo_map);
    test_notes_free(&notes);
}

static void test_pickup(void) {
    TestNotes notes;
    test_notes_init(&notes);
    /* a quarter-note pickup (1/4 signature), then 4/4 from 1/4 */
    add_line(&notes, 0, 1, 1, 4, 9, 60, "melody");
    const TestMeterChange meters[] = {{1, 4, 0, 1}, {4, 4, 1, 4}};
    MeterMap *meter_map = test_meter_map(meters, 2);
    assert(meter_map_has_pickup(meter_map));
    test_round_trip(notes.events, notes.pitches, notes.count, meter_map, NULL, NULL, 0, 480);
    meter_map_destroy(meter_map);
    test_notes_free(&notes);
}

int main(void) {
    test_reference_pieces();
    test_three_four();
    test_six_eight();
    test_twelve_eight();
    test_meter_and_tempo_changes();
    test_pickup();
    printf("test_midi_renderer_roundtrip: all tests passed\n");
    return 0;
}
