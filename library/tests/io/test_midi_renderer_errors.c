#include "midi_render_test_util.h"

/* What cannot be rendered exactly, argument checks, untouched outputs, determinism and the voice limit. */

#define SENTINEL_DATA ((unsigned char *)(uintptr_t)0x1)
#define SENTINEL_SIZE ((size_t)12345)

static MidiRenderResult render_notes(const TestNotes *notes, unsigned division) {
    unsigned char *data = SENTINEL_DATA;
    size_t size = SENTINEL_SIZE;
    MidiRenderResult result =
        midi_renderer_render(notes->events, notes->pitches, notes->count, NULL, NULL, NULL, 0, division, &data, &size);
    if (result != MIDI_RENDER_OK) {
        assert(data == SENTINEL_DATA && size == SENTINEL_SIZE);
    } else {
        free(data);
    }
    return result;
}

static void test_null_arguments(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, NULL);
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, 1, NULL, NULL, NULL, 0, 480, NULL, &size) ==
           MIDI_RENDER_ERR_NULL_ARG);
    assert(midi_renderer_render(notes.events, notes.pitches, 1, NULL, NULL, NULL, 0, 480, &data, NULL) ==
           MIDI_RENDER_ERR_NULL_ARG);
    assert(midi_renderer_render(NULL, notes.pitches, 1, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_ERR_NULL_ARG);
    assert(midi_renderer_render(notes.events, NULL, 1, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_ERR_NULL_ARG);
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, NULL, 1, 480, &data, &size) == MIDI_RENDER_ERR_NULL_ARG);
    assert(data == NULL && size == 0);
    test_notes_free(&notes);
}

static void test_bad_division(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, NULL);
    assert(render_notes(&notes, 0) == MIDI_RENDER_ERR_BAD_DIVISION);
    assert(render_notes(&notes, MIDI_RENDER_MAX_DIVISION + 1) == MIDI_RENDER_ERR_BAD_DIVISION);
    assert(render_notes(&notes, 0x8000 | 0xE728) == MIDI_RENDER_ERR_BAD_DIVISION);
    assert(render_notes(&notes, 1) == MIDI_RENDER_OK);
    test_notes_free(&notes);
}

static void test_inexact_ticks(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 1, 7, 1, 4, 60, NULL); /* 4 * 480 / 7 ticks */
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    assert(render_notes(&notes, 7) == MIDI_RENDER_OK); /* 4 ticks at division 7 */
    test_notes_free(&notes);

    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 7, 60, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    /* a sixty-fourth note is 30 ticks at 480 but half a tick at division 8 */
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 64, 60, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_OK);
    assert(render_notes(&notes, 8) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);
}

static void test_bad_values(void) {
    TestNotes notes;

    test_notes_init(&notes);
    test_notes_add(&notes, -1, 4, 1, 4, 60, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 128, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, -1, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    test_notes_init(&notes);
    test_notes_add_exact(&notes, 0, 1, 1, 4, 121, 2, NULL); /* 60.5: a microtone */
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 0, NULL);
    test_notes_add(&notes, 1, 4, 1, 4, 127, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_OK);
    test_notes_free(&notes);

    /* a tick beyond the 28-bit delta-time range */
    test_notes_init(&notes);
    test_notes_add(&notes, 1000000, 1, 1, 4, 60, NULL);
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);
}

static void test_same_pitch_overlap(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 2, 60, "v");
    test_notes_add(&notes, 1, 4, 1, 2, 60, "v");
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    test_notes_free(&notes);

    /* the same overlap in different voices is two tracks, which is fine */
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 2, 60, "v");
    test_notes_add(&notes, 1, 4, 1, 2, 60, "w");
    assert(render_notes(&notes, 480) == MIDI_RENDER_OK);
    test_notes_free(&notes);

    /* different pitches overlap freely */
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 2, 60, "v");
    test_notes_add(&notes, 1, 4, 1, 2, 64, "v");
    assert(render_notes(&notes, 480) == MIDI_RENDER_OK);
    test_notes_free(&notes);
}

static void test_map_and_key_limits(void) {
    unsigned char *data = SENTINEL_DATA;
    size_t size = SENTINEL_SIZE;

    /* the widest signature a TimeSignature allows (64/16) is written */
    const TestMeterChange wide[] = {{64, 16, 0, 1}};
    MeterMap *meter_map = test_meter_map(wide, 1);
    assert(midi_renderer_render(NULL, NULL, 0, meter_map, NULL, NULL, 0, 480, &data, &size) == MIDI_RENDER_OK);
    free(data);
    data = SENTINEL_DATA;
    size = SENTINEL_SIZE;
    meter_map_destroy(meter_map);

    const TestMeterChange offgrid[] = {{4, 4, 0, 1}, {3, 4, 1, 7}};
    meter_map = test_meter_map(offgrid, 2);
    assert(midi_renderer_render(NULL, NULL, 0, meter_map, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    assert(data == SENTINEL_DATA && size == SENTINEL_SIZE);
    meter_map_destroy(meter_map);

    TempoMap *tempo_map = NULL;
    {
        MusicalTime *start = test_time(0, 1);
        TempoMapChangePoint point = {start, 0x1000000UL};
        TempoMapResult created = tempo_map_create(test_id("test-tempo-map"), &point, 1, &tempo_map);
        musical_time_destroy(start);
        if (created == TEMPO_MAP_OK) {
            assert(midi_renderer_render(NULL, NULL, 0, NULL, tempo_map, NULL, 0, 480, &data, &size) ==
                   MIDI_RENDER_ERR_NOT_REPRESENTABLE);
            assert(data == SENTINEL_DATA && size == SENTINEL_SIZE);
            tempo_map_destroy(tempo_map);
        }
    }

    MidiKeySignatureEvent key;
    memset(&key, 0, sizeof(key));
    key.position = test_time(0, 1);
    key.sharps_flats = 8;
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, &key, 1, 480, &data, &size) ==
           MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    key.sharps_flats = -8;
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, &key, 1, 480, &data, &size) ==
           MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    musical_time_destroy(key.position);
    key.position = test_time(1, 7);
    key.sharps_flats = 1;
    assert(midi_renderer_render(NULL, NULL, 0, NULL, NULL, &key, 1, 480, &data, &size) ==
           MIDI_RENDER_ERR_NOT_REPRESENTABLE);
    assert(data == SENTINEL_DATA && size == SENTINEL_SIZE);
    musical_time_destroy(key.position);
}

static void add_voices(TestNotes *notes, int voices) {
    for (int v = 0; v < voices; v++) {
        char name[32];
        snprintf(name, sizeof(name), "voice-%02d", v);
        test_notes_add(notes, v, 4, 1, 4, 40 + v, name);
    }
}

static void test_voice_limit_and_channels(void) {
    TestNotes notes;
    test_notes_init(&notes);
    add_voices(&notes, 15);
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    assert(smf.track_count == 16);
    for (size_t t = 1; t <= 15; t++) {
        int channel = -1;
        assert(test_track_note_ons(&smf.tracks[t], &channel) == 1);
        int expected = (int)t - 1;
        if (expected >= 9) {
            expected++; /* channel 9 is percussion */
        }
        assert(channel == expected && channel != 9);
    }
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);

    test_notes_init(&notes);
    add_voices(&notes, 16);
    assert(render_notes(&notes, 480) == MIDI_RENDER_ERR_TOO_MANY_VOICES);
    test_notes_free(&notes);
}

static void test_deterministic(void) {
    TestNotes notes;
    test_notes_init(&notes);
    add_voices(&notes, 4);
    test_notes_add(&notes, 0, 1, 1, 8, 70, NULL);
    const TestMeterChange meters[] = {{3, 4, 0, 1}};
    const TestTempoChange tempos[] = {{0, 1, 600000}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    TempoMap *tempo_map = test_tempo_map(tempos, 1);
    unsigned char *first = NULL;
    unsigned char *second = NULL;
    size_t first_size = 0;
    size_t second_size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, meter_map, tempo_map, NULL, 0, 960, &first,
                                &first_size) == MIDI_RENDER_OK);
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, meter_map, tempo_map, NULL, 0, 960, &second,
                                &second_size) == MIDI_RENDER_OK);
    assert(first_size == second_size && memcmp(first, second, first_size) == 0);
    free(first);
    free(second);
    meter_map_destroy(meter_map);
    tempo_map_destroy(tempo_map);
    test_notes_free(&notes);
}

int main(void) {
    test_null_arguments();
    test_bad_division();
    test_inexact_ticks();
    test_bad_values();
    test_same_pitch_overlap();
    test_map_and_key_limits();
    test_voice_limit_and_channels();
    test_deterministic();
    printf("test_midi_renderer_errors: all tests passed\n");
    return 0;
}
