#include "midi_render_test_util.h"

/*
 * Round trips: the reference pieces of the framework-v1-corpus (import -> render -> import).
 * URSATZ_CORPUS_DIR is passed in by CMake.
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

int main(void) {
    test_reference_pieces();
    printf("test_midi_renderer_roundtrip: all tests passed\n");
    return 0;
}
