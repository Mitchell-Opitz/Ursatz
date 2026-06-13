#include "midi_render_test_util.h"

/* Track names (0xFF 0x03) and markers (0xFF 0x06): where the bytes go, the errors, and that the importer ignores them. */

#define SENTINEL_DATA ((unsigned char *)(uintptr_t)0x1)
#define SENTINEL_SIZE ((size_t)12345)

/* The longest text a variable-length quantity can length: one byte more is not representable. */
#define TEXT_LIMIT 0x0FFFFFFFu

static void assert_bytes(const unsigned char *data, size_t size, const unsigned char *expected, size_t expected_size) {
    assert(data != NULL);
    assert(size == expected_size);
    assert(memcmp(data, expected, expected_size) == 0);
}

static void render_options(const TestNotes *notes, const MeterMap *meter_map, const TempoMap *tempo_map,
                           const MidiKeySignatureEvent *keys, size_t key_count, const MidiRenderOptions *options,
                           unsigned char **data, size_t *size) {
    assert(midi_renderer_render_with_options(notes->events, notes->pitches, notes->count, meter_map, tempo_map, keys,
                                             key_count, options, 480, data, size) == MIDI_RENDER_OK);
}

/* Two voices and a conductor name: byte-exact. A name for a voice with no notes is ignored; entries match by voice id. */
static void test_names_two_voices(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "v-melody");
    test_notes_add(&notes, 0, 1, 1, 2, 48, "v-bass");

    const MidiTrackName names[] = {{"v-bass", "bass"}, {"v-ghost", "ghost"}, {"v-melody", "melody"}};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.conductor_name = "piece";
    options.track_names = names;
    options.track_name_count = 3;

    unsigned char *data = NULL;
    size_t size = 0;
    render_options(&notes, NULL, NULL, NULL, 0, &options, &data, &size);
    static const unsigned char expected[] = {
        'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x03, 0x01, 0xE0,
        'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x0D,
        0x00, 0xFF, 0x03, 0x05, 'p',  'i',  'e',  'c',  'e',
        0x00, 0xFF, 0x2F, 0x00,
        'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x17,
        0x00, 0xFF, 0x03, 0x06, 'm',  'e',  'l',  'o',  'd',  'y',
        0x00, 0x90, 0x3C, 0x40,
        0x83, 0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
        'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x15,
        0x00, 0xFF, 0x03, 0x04, 'b',  'a',  's',  's',
        0x00, 0x91, 0x30, 0x40,
        0x87, 0x40, 0x81, 0x30, 0x00,
        0x00, 0xFF, 0x2F, 0x00};
    assert_bytes(data, size, expected, sizeof(expected));
    free(data);
    test_notes_free(&notes);
}

/* The notes with no voice are one track and take the name whose voice_id is NULL. */
static void test_name_for_notes_without_a_voice(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, NULL);

    const MidiTrackName names[] = {{NULL, "loose"}};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.track_names = names;
    options.track_name_count = 1;

    unsigned char *data = NULL;
    size_t size = 0;
    render_options(&notes, NULL, NULL, NULL, 0, &options, &data, &size);
    static const unsigned char expected[] = {
        'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x02, 0x01, 0xE0,
        'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x04,
        0x00, 0xFF, 0x2F, 0x00,
        'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x16,
        0x00, 0xFF, 0x03, 0x05, 'l',  'o',  'o',  's',  'e',
        0x00, 0x90, 0x3C, 0x40,
        0x83, 0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00};
    assert_bytes(data, size, expected, sizeof(expected));
    free(data);
    test_notes_free(&notes);
}

/* Markers at 0 and at bar 3 of 3/4, after the tempo, time signature and key signature at the same tick, supplied
   out of tick order and in order at an equal tick. */
static void test_markers(void) {
    const TestMeterChange meters[] = {{3, 4, 0, 1}};
    const TestTempoChange tempos[] = {{0, 1, 500000}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    TempoMap *tempo_map = test_tempo_map(tempos, 1);
    MidiKeySignatureEvent keys[1];
    memset(keys, 0, sizeof(keys));
    keys[0].position = test_time(0, 1);
    keys[0].sharps_flats = -2;
    keys[0].mode_byte = 0;

    MusicalTime *bar_three = test_time(3, 2); /* two bars of 3/4 */
    MusicalTime *zero = test_time(0, 1);
    const MidiMarker markers[] = {{bar_three, "B"}, {zero, "x"}, {zero, "y"}};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.markers = markers;
    options.marker_count = 3;

    TestNotes notes;
    test_notes_init(&notes);
    unsigned char *data = NULL;
    size_t size = 0;
    render_options(&notes, meter_map, tempo_map, keys, 1, &options, &data, &size);
    static const unsigned char expected[] = {
        'M',  'T',  'h',  'd',  0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x01, 0xE0,
        'M',  'T',  'r',  'k',  0x00, 0x00, 0x00, 0x29,
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x59, 0x02, 0xFE, 0x00,
        0x00, 0xFF, 0x06, 0x01, 'x',
        0x00, 0xFF, 0x06, 0x01, 'y',
        0x96, 0x40, 0xFF, 0x06, 0x01, 'B',
        0x00, 0xFF, 0x2F, 0x00};
    assert_bytes(data, size, expected, sizeof(expected));
    free(data);

    test_notes_free(&notes);
    musical_time_destroy(bar_three);
    musical_time_destroy(zero);
    musical_time_destroy(keys[0].position);
    meter_map_destroy(meter_map);
    tempo_map_destroy(tempo_map);
}

/* NULL options give exactly the bytes of the old function, on a piece with two voices, maps and a key signature. */
static void test_null_options_equal_the_old_function(void) {
    const TestMeterChange meters[] = {{6, 8, 0, 1}, {4, 4, 3, 2}};
    const TestTempoChange tempos[] = {{0, 1, 500000}, {1, 1, 400000}};
    MeterMap *meter_map = test_meter_map(meters, 2);
    TempoMap *tempo_map = test_tempo_map(tempos, 2);
    MidiKeySignatureEvent keys[1];
    memset(keys, 0, sizeof(keys));
    keys[0].position = test_time(1, 1);
    keys[0].sharps_flats = 3;
    keys[0].mode_byte = 1;

    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "v1");
    test_notes_add(&notes, 1, 4, 1, 4, 62, "v1");
    test_notes_add(&notes, 0, 1, 1, 2, 48, "v2");
    test_notes_add(&notes, 3, 4, 1, 8, 50, NULL);

    unsigned char *old_data = NULL;
    size_t old_size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, meter_map, tempo_map, keys, 1, 480, &old_data,
                                &old_size) == MIDI_RENDER_OK);
    unsigned char *new_data = NULL;
    size_t new_size = 0;
    render_options(&notes, meter_map, tempo_map, keys, 1, NULL, &new_data, &new_size);
    assert_bytes(new_data, new_size, old_data, old_size);
    free(new_data);

    MidiRenderOptions empty;
    memset(&empty, 0, sizeof(empty));
    render_options(&notes, meter_map, tempo_map, keys, 1, &empty, &new_data, &new_size);
    assert_bytes(new_data, new_size, old_data, old_size);
    free(new_data);
    free(old_data);

    test_notes_free(&notes);
    musical_time_destroy(keys[0].position);
    meter_map_destroy(meter_map);
    tempo_map_destroy(tempo_map);
}

static MidiRenderResult render_checked(const TestNotes *notes, const MidiRenderOptions *options) {
    unsigned char *data = SENTINEL_DATA;
    size_t size = SENTINEL_SIZE;
    MidiRenderResult result = midi_renderer_render_with_options(notes->events, notes->pitches, notes->count, NULL, NULL,
                                                                NULL, 0, options, 480, &data, &size);
    if (result != MIDI_RENDER_OK) {
        assert(data == SENTINEL_DATA && size == SENTINEL_SIZE); /* nothing is written on error */
    } else {
        free(data);
    }
    return result;
}

static void test_errors(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, "v1");
    MusicalTime *zero = test_time(0, 1);
    MusicalTime *negative = test_time(-1, 4);
    MusicalTime *off_tick = test_time(1, 7); /* 1920 / 7 ticks */
    MidiRenderOptions options;

    /* NULL name, text, position, and counts with NULL arrays */
    const MidiTrackName null_name[] = {{"v1", NULL}};
    memset(&options, 0, sizeof(options));
    options.track_names = null_name;
    options.track_name_count = 1;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NULL_ARG);

    memset(&options, 0, sizeof(options));
    options.track_name_count = 1;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NULL_ARG);

    const MidiMarker null_text[] = {{zero, NULL}};
    memset(&options, 0, sizeof(options));
    options.markers = null_text;
    options.marker_count = 1;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NULL_ARG);

    const MidiMarker null_position[] = {{NULL, "A"}};
    options.markers = null_position;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NULL_ARG);

    options.markers = NULL;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NULL_ARG);

    /* two names for one voice, including two for the notes with no voice */
    const MidiTrackName twice[] = {{"v1", "a"}, {"v1", "b"}};
    memset(&options, 0, sizeof(options));
    options.track_names = twice;
    options.track_name_count = 2;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

    const MidiTrackName twice_none[] = {{NULL, "a"}, {NULL, "b"}};
    options.track_names = twice_none;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

    /* a marker position that is negative, or not a whole tick */
    const MidiMarker at_negative[] = {{negative, "A"}};
    memset(&options, 0, sizeof(options));
    options.markers = at_negative;
    options.marker_count = 1;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

    const MidiMarker at_off_tick[] = {{off_tick, "A"}};
    options.markers = at_off_tick;
    assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

    /* a name or text longer than 0x0FFFFFFF bytes */
    char *huge = (char *)malloc((size_t)TEXT_LIMIT + 2);
    if (huge != NULL) {
        memset(huge, 'a', (size_t)TEXT_LIMIT + 1);
        huge[(size_t)TEXT_LIMIT + 1] = '\0';

        memset(&options, 0, sizeof(options));
        options.conductor_name = huge;
        assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

        const MidiTrackName long_name[] = {{"v1", huge}};
        memset(&options, 0, sizeof(options));
        options.track_names = long_name;
        options.track_name_count = 1;
        assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

        const MidiMarker long_text[] = {{zero, huge}};
        memset(&options, 0, sizeof(options));
        options.markers = long_text;
        options.marker_count = 1;
        assert(render_checked(&notes, &options) == MIDI_RENDER_ERR_NOT_REPRESENTABLE);

        /* the limit itself is representable */
        huge[TEXT_LIMIT] = '\0';
        assert(render_checked(&notes, &options) == MIDI_RENDER_OK);
        free(huge);
    }

    musical_time_destroy(zero);
    musical_time_destroy(negative);
    musical_time_destroy(off_tick);
    test_notes_free(&notes);
}

/* The importer ignores both meta-events: the same notes, meter and tempo with and without them. */
static void test_import_ignores_names_and_markers(void) {
    const TestMeterChange meters[] = {{3, 4, 0, 1}};
    const TestTempoChange tempos[] = {{0, 1, 500000}, {3, 2, 400000}};
    MeterMap *meter_map = test_meter_map(meters, 1);
    TempoMap *tempo_map = test_tempo_map(tempos, 2);

    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 72, "v-melody");
    test_notes_add(&notes, 1, 4, 1, 2, 74, "v-melody");
    test_notes_add(&notes, 0, 1, 3, 4, 48, "v-bass");
    test_notes_add(&notes, 3, 4, 3, 4, 43, "v-bass");

    const MidiTrackName names[] = {{"v-melody", "melody"}, {"v-bass", "bass"}};
    MusicalTime *zero = test_time(0, 1);
    MusicalTime *bar_three = test_time(3, 2);
    const MidiMarker markers[] = {{zero, "A"}, {bar_three, "B"}};
    MidiRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.conductor_name = "piece";
    options.track_names = names;
    options.track_name_count = 2;
    options.markers = markers;
    options.marker_count = 2;

    unsigned char *plain = NULL;
    size_t plain_size = 0;
    render_options(&notes, meter_map, tempo_map, NULL, 0, NULL, &plain, &plain_size);
    unsigned char *named = NULL;
    size_t named_size = 0;
    render_options(&notes, meter_map, tempo_map, NULL, 0, &options, &named, &named_size);
    assert(named_size > plain_size);

    /* the named file is a valid SMF with the same tracks, and the notes round-trip */
    TestSmf smf;
    test_smf_parse(named, named_size, &smf);
    assert(smf.track_count == 3);
    free(smf.tracks);

    NoteEvent **plain_events = NULL;
    PitchNumericValue **plain_pitches = NULL;
    size_t plain_count = 0;
    MidiMeterImport plain_meter = {NULL, NULL, NULL, 0};
    assert(midi_importer_import_with_meter(plain, plain_size, &plain_events, &plain_pitches, &plain_count,
                                           &plain_meter) == MIDI_IMPORT_OK);
    NoteEvent **named_events = NULL;
    PitchNumericValue **named_pitches = NULL;
    size_t named_count = 0;
    MidiMeterImport named_meter = {NULL, NULL, NULL, 0};
    assert(midi_importer_import_with_meter(named, named_size, &named_events, &named_pitches, &named_count,
                                           &named_meter) == MIDI_IMPORT_OK);
    assert(plain_count == notes.count && named_count == plain_count);
    TestNoteFact *plain_facts = test_facts_of(plain_events, plain_pitches, plain_count);
    TestNoteFact *named_facts = test_facts_of(named_events, named_pitches, named_count);
    for (size_t i = 0; i < plain_count; i++) {
        assert(test_compare_facts(&plain_facts[i], &named_facts[i]) == 0);
    }
    test_assert_same_meter(plain_meter.meter_map, named_meter.meter_map);
    free(plain_facts);
    free(named_facts);
    for (size_t i = 0; i < plain_count; i++) {
        note_event_destroy(plain_events[i]);
        pitch_numeric_value_destroy(plain_pitches[i]);
        note_event_destroy(named_events[i]);
        pitch_numeric_value_destroy(named_pitches[i]);
    }
    free(plain_events);
    free(plain_pitches);
    free(named_events);
    free(named_pitches);
    midi_meter_import_destroy(&plain_meter);
    midi_meter_import_destroy(&named_meter);

    MidiTempoImport plain_tempo = {NULL, NULL, 0};
    MidiTempoImport named_tempo = {NULL, NULL, 0};
    assert(midi_importer_import_with_tempo(plain, plain_size, &plain_events, &plain_pitches, &plain_count,
                                           &plain_tempo) == MIDI_IMPORT_OK);
    assert(midi_importer_import_with_tempo(named, named_size, &named_events, &named_pitches, &named_count,
                                           &named_tempo) == MIDI_IMPORT_OK);
    test_assert_same_tempo(plain_tempo.tempo_map, named_tempo.tempo_map);
    test_assert_same_tempo(tempo_map, named_tempo.tempo_map);
    for (size_t i = 0; i < plain_count; i++) {
        note_event_destroy(plain_events[i]);
        pitch_numeric_value_destroy(plain_pitches[i]);
    }
    for (size_t i = 0; i < named_count; i++) {
        note_event_destroy(named_events[i]);
        pitch_numeric_value_destroy(named_pitches[i]);
    }
    free(plain_events);
    free(plain_pitches);
    free(named_events);
    free(named_pitches);
    midi_tempo_import_destroy(&plain_tempo);
    midi_tempo_import_destroy(&named_tempo);

    free(plain);
    free(named);
    musical_time_destroy(zero);
    musical_time_destroy(bar_three);
    test_notes_free(&notes);
    meter_map_destroy(meter_map);
    tempo_map_destroy(tempo_map);
}

int main(void) {
    test_names_two_voices();
    test_name_for_notes_without_a_voice();
    test_markers();
    test_null_options_equal_the_old_function();
    test_errors();
    test_import_ignores_names_and_markers();
    printf("test_midi_renderer_meta: all tests passed\n");
    return 0;
}
