#include "midi_render_test_util.h"

/* Header, conductor track and zero notes: byte-exact. */

static void assert_bytes(const unsigned char *data, size_t size, const unsigned char *expected, size_t expected_size) {
    assert(data != NULL);
    assert(size == expected_size);
    assert(memcmp(data, expected, expected_size) == 0);
}

static void test_empty_file(void) {
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, NULL, 0, 480, &data, &size) == MIDI_RENDER_OK);
    static const unsigned char expected[] = {'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x01,
                                             0xE0, 'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x04, 0x00, 0xFF, 0x2F, 0x00};
    assert_bytes(data, size, expected, sizeof(expected));
    free(data);
}

static void test_division_is_written(void) {
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, NULL, 0, 96, &data, &size) == MIDI_RENDER_OK);
    assert(size == 26 && data[12] == 0x00 && data[13] == 0x60);
    free(data);
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, NULL, 0, MIDI_RENDER_MAX_DIVISION, &data, &size) ==
           MIDI_RENDER_OK);
    assert(data[12] == 0x7F && data[13] == 0xFF);
    free(data);
}

static void test_conductor_track(void) {
    const TestMeterChange meters[] = {{6, 8, 0, 1}, {4, 4, 3, 2}};
    const TestTempoChange tempos[] = {{0, 1, 500000}, {1, 1, 400000}};
    MeterMap *meter_map = test_meter_map(meters, 2);
    TempoMap *tempo_map = test_tempo_map(tempos, 2);

    MidiKeySignatureEvent keys[2];
    memset(keys, 0, sizeof(keys));
    keys[0].position = test_time(0, 1);
    keys[0].sharps_flats = -2;
    keys[0].mode_byte = 0;
    keys[1].position = test_time(1, 1);
    keys[1].sharps_flats = 3;
    keys[1].mode_byte = 1;

    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(NULL, NULL, 0, meter_map, tempo_map, keys, 2, 480, &data, &size) == MIDI_RENDER_OK);
    static const unsigned char expected[] = {
        'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x01, 0xE0, 'M',  'T',  'r',
        'k',  0x00, 0x00, 0x00, 0x30,
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0xFF, 0x58, 0x04, 0x06, 0x03, 0x18, 0x08,
        0x00, 0xFF, 0x59, 0x02, 0xFE, 0x00,
        0x8F, 0x00, 0xFF, 0x51, 0x03, 0x06, 0x1A, 0x80,
        0x00, 0xFF, 0x59, 0x02, 0x03, 0x01,
        0x87, 0x40, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x2F, 0x00};
    assert_bytes(data, size, expected, sizeof(expected));
    free(data);

    musical_time_destroy(keys[0].position);
    musical_time_destroy(keys[1].position);
    meter_map_destroy(meter_map);
    tempo_map_destroy(tempo_map);
}

static void test_zero_notes_with_maps_parses(void) {
    const TestMeterChange meters[] = {{3, 4, 0, 1}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    test_round_trip(NULL, NULL, 0, meter_map, NULL, NULL, 0, 480);
    meter_map_destroy(meter_map);
}

int main(void) {
    test_empty_file();
    test_division_is_written();
    test_conductor_track();
    test_zero_notes_with_maps_parses();
    printf("test_midi_renderer: all tests passed\n");
    return 0;
}
