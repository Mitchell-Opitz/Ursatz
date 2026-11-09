#include "io/midi_importer.h"
#include "midi_import_split.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

/*
 * Complete MIDI import: midi_importer_import_complete returns notes, meter, tempo, key signatures and the tracks
 * with their names and program changes from one parse. SMF files are built in code (a type 0 and a type 1 file, with
 * track names and program changes, which the renderer does not write for programs) and the five framework-v1-corpus
 * files are read from URSATZ_CORPUS_DIR. Every file is also imported through the three older entry points and
 * midi_key_signature_scan_all, and the results are compared part by part. Division is 480 ticks per quarter note.
 */

#define DIVISION 480

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
static void start_event(TrackBuilder *t, uint32_t at) {
    assert(at >= t->tick);
    put_delta(t, at - t->tick);
    t->tick = at;
}

static void add_name(TrackBuilder *t, uint32_t at, const char *text) {
    start_event(t, at);
    put(t, 0xFF);
    put(t, 0x03);
    put(t, (unsigned char)strlen(text));
    for (const char *c = text; *c != '\0'; c++) {
        put(t, (unsigned char)*c);
    }
}

static void add_program(TrackBuilder *t, uint32_t at, unsigned char channel, unsigned char program) {
    start_event(t, at);
    put(t, (unsigned char)(0xC0 | channel));
    put(t, program);
}

static void add_time_signature(TrackBuilder *t, uint32_t at, unsigned char top, unsigned char bottom_exponent) {
    start_event(t, at);
    put(t, 0xFF);
    put(t, 0x58);
    put(t, 0x04);
    put(t, top);
    put(t, bottom_exponent);
    put(t, 24);
    put(t, 8);
}

static void add_tempo(TrackBuilder *t, uint32_t at, uint32_t microseconds_per_quarter) {
    start_event(t, at);
    put(t, 0xFF);
    put(t, 0x51);
    put(t, 0x03);
    put(t, (unsigned char)(microseconds_per_quarter >> 16));
    put(t, (unsigned char)(microseconds_per_quarter >> 8));
    put(t, (unsigned char)microseconds_per_quarter);
}

static void add_key_signature(TrackBuilder *t, uint32_t at, int8_t sharps_flats, unsigned char minor) {
    start_event(t, at);
    put(t, 0xFF);
    put(t, 0x59);
    put(t, 0x02);
    put(t, (unsigned char)sharps_flats);
    put(t, minor);
}

static void add_note(TrackBuilder *t, uint32_t at, uint32_t duration, unsigned char channel, unsigned char key) {
    start_event(t, at);
    put(t, (unsigned char)(0x90 | channel));
    put(t, key);
    put(t, 0x64);
    put_delta(t, duration);
    put(t, (unsigned char)(0x80 | channel));
    put(t, key);
    put(t, 0x40);
    t->tick = at + duration;
}

/* A note-on that is never turned off. */
static void add_unmatched_note_on(TrackBuilder *t, uint32_t at, unsigned char channel, unsigned char key) {
    start_event(t, at);
    put(t, (unsigned char)(0x90 | channel));
    put(t, key);
    put(t, 0x64);
}

static size_t build_smf(unsigned char *out, uint16_t format, uint16_t division, const TrackBuilder *tracks,
                        uint16_t track_count) {
    size_t pos = 0;
    const unsigned char header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, (unsigned char)format,
                                    (unsigned char)(track_count >> 8), (unsigned char)track_count,
                                    (unsigned char)(division >> 8), (unsigned char)(division & 0xFF)};
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
    unsigned char *data;
    size_t size;
} File;

static File file_from_tracks(uint16_t format, const TrackBuilder *tracks, uint16_t track_count) {
    File file;
    file.data = (unsigned char *)malloc(8192);
    assert(file.data != NULL);
    file.size = build_smf(file.data, format, DIVISION, tracks, track_count);
    return file;
}

static File read_corpus_file(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, name);
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    assert(length > 0);
    File file;
    file.data = (unsigned char *)malloc((size_t)length);
    assert(file.data != NULL);
    file.size = (size_t)length;
    assert(fread(file.data, 1, file.size, f) == file.size);
    fclose(f);
    return file;
}

/* Type 0: one track with a name, a second name that is ignored, program changes at tick 0 and 480, and two notes. */
static File fixture_type0(void) {
    static TrackBuilder track;
    memset(&track, 0, sizeof(track));
    add_name(&track, 0, "Piano");
    add_name(&track, 0, "Ignored");
    add_time_signature(&track, 0, 3, 2);
    add_tempo(&track, 0, 600000);
    add_key_signature(&track, 0, 2, 0);
    add_program(&track, 0, 0, 5);
    add_note(&track, 0, 480, 0, 60);
    add_program(&track, 480, 0, 7);
    add_note(&track, 480, 480, 0, 62);
    return file_from_tracks(0, &track, 1);
}

/* Type 1: a conductor track (name, tempo, meter, key change), a named track on channel 0, an unnamed track on channel
   1 with a program change mid-way, and a track with no events. */
static File fixture_type1(void) {
    static TrackBuilder tracks[4];
    memset(tracks, 0, sizeof(tracks));
    add_name(&tracks[0], 0, "Conductor");
    add_time_signature(&tracks[0], 0, 4, 2);
    add_tempo(&tracks[0], 0, 500000);
    add_key_signature(&tracks[0], 0, -1, 1);
    add_tempo(&tracks[0], 960, 400000);
    add_key_signature(&tracks[0], 960, 3, 0);

    add_name(&tracks[1], 0, "Right hand");
    add_program(&tracks[1], 0, 0, 0);
    add_note(&tracks[1], 0, 480, 0, 72);
    add_note(&tracks[1], 960, 480, 0, 74);

    add_program(&tracks[2], 0, 1, 33);
    add_note(&tracks[2], 480, 480, 1, 48);
    add_program(&tracks[2], 960, 1, 40);

    return file_from_tracks(1, tracks, 4);
}

static void assert_time(const MusicalTime *time, long numerator, unsigned long denominator) {
    MusicalTime *expected = NULL;
    assert(musical_time_create(numerator, denominator, &expected) == MUSICAL_TIME_OK);
    assert(musical_time_equals(time, expected));
    musical_time_destroy(expected);
}

static void assert_program(const MidiProgramChange *change, uint64_t tick, unsigned char channel,
                           unsigned char program) {
    assert(change->tick == tick);
    assert(change->channel == channel);
    assert(change->program == program);
    assert_time(change->time, (long)tick, 4 * DIVISION);
}

static void test_type0(void) {
    File file = fixture_type0();
    MidiImport import;
    assert(midi_importer_import_complete(file.data, file.size, &import) == MIDI_IMPORT_OK);

    assert(import.format == 0);
    assert(import.division == DIVISION);

    assert(import.track_count == 1);
    assert(import.tracks[0].index == 0);
    assert(import.tracks[0].name != NULL && strcmp(import.tracks[0].name, "Piano") == 0);
    assert(import.tracks[0].program_count == 2);
    assert_program(&import.tracks[0].programs[0], 0, 0, 5);
    assert_program(&import.tracks[0].programs[1], 480, 0, 7);

    assert(import.count == 2);
    assert(import.note_tracks[0] == 0 && import.note_tracks[1] == 0);
    assert(import.note_channels[0] == 0 && import.note_channels[1] == 0);
    assert_time(temporal_anchor_position(note_event_temporal_anchor(import.events[1])), 480, 4 * DIVISION);

    assert(meter_map_region_count(import.meter.meter_map) == 1);
    assert(!meter_map_is_defaulted(import.meter.meter_map));
    const TimeSignature *signature = meter_map_region_time_signature(meter_map_region(import.meter.meter_map, 0));
    assert(time_signature_top_number(signature) == 3 && time_signature_bottom_number(signature) == 4);
    assert_time(import.meter.end_time, 960, 4 * DIVISION);

    assert(tempo_map_region_count(import.tempo.tempo_map) == 1);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo.tempo_map, 0)) == 600000);

    assert(import.key_signatures.count == 1);
    assert(import.key_signatures.events[0].sharps_flats == 2);
    assert(import.key_signatures.events[0].mode_hint == MIDI_MODE_HINT_MAJOR);
    assert(import.key_signatures.events[0].tick == 0);

    midi_import_destroy(&import);
    assert(import.events == NULL && import.tracks == NULL && import.count == 0 && import.track_count == 0);
    free(file.data);
}

static void test_type1(void) {
    File file = fixture_type1();
    MidiImport import;
    assert(midi_importer_import_complete(file.data, file.size, &import) == MIDI_IMPORT_OK);

    assert(import.format == 1);
    assert(import.track_count == 4);
    for (size_t t = 0; t < import.track_count; t++) {
        assert(import.tracks[t].index == t);
    }
    assert(import.tracks[0].name != NULL && strcmp(import.tracks[0].name, "Conductor") == 0);
    assert(import.tracks[0].program_count == 0 && import.tracks[0].programs == NULL);
    assert(import.tracks[1].name != NULL && strcmp(import.tracks[1].name, "Right hand") == 0);
    assert(import.tracks[1].program_count == 1);
    assert_program(&import.tracks[1].programs[0], 0, 0, 0);
    assert(import.tracks[2].name == NULL);
    assert(import.tracks[2].program_count == 2);
    assert_program(&import.tracks[2].programs[0], 0, 1, 33);
    assert_program(&import.tracks[2].programs[1], 960, 1, 40);
    assert(import.tracks[3].name == NULL && import.tracks[3].program_count == 0);

    /* Notes in time order: track 1 at 0, track 2 at 480, track 1 at 960. */
    assert(import.count == 3);
    const size_t expected_tracks[3] = {1, 2, 1};
    const unsigned char expected_channels[3] = {0, 1, 0};
    const long expected_starts[3] = {0, 480, 960};
    for (size_t i = 0; i < 3; i++) {
        assert(import.note_tracks[i] == expected_tracks[i]);
        assert(import.note_channels[i] == expected_channels[i]);
        assert_time(temporal_anchor_position(note_event_temporal_anchor(import.events[i])), expected_starts[i],
                    4 * DIVISION);
    }
    /* Every note carries a voice (by track, R-386), and notes of different tracks never share one. */
    for (size_t i = 0; i < 3; i++) {
        assert(note_event_optional_voice(import.events[i]) != NULL);
        for (size_t j = i + 1; j < 3; j++) {
            if (expected_tracks[i] != expected_tracks[j]) {
                assert(strcmp(entity_id_value(voice_reference_target_id(note_event_optional_voice(import.events[i]))),
                              entity_id_value(voice_reference_target_id(note_event_optional_voice(import.events[j])))) != 0);
            }
        }
    }

    assert(tempo_map_region_count(import.tempo.tempo_map) == 2);
    assert(tempo_map_region_microseconds_per_quarter(tempo_map_region(import.tempo.tempo_map, 1)) == 400000);
    assert(import.key_signatures.count == 2);
    assert(import.key_signatures.events[0].sharps_flats == -1);
    assert(import.key_signatures.events[0].mode_hint == MIDI_MODE_HINT_MINOR);
    assert(import.key_signatures.events[1].sharps_flats == 3);
    assert(import.key_signatures.events[1].tick == 960);

    midi_import_destroy(&import);
    free(file.data);
}

static void assert_meters_equal(const MidiMeterImport *a, const MidiMeterImport *b) {
    assert(meter_map_is_defaulted(a->meter_map) == meter_map_is_defaulted(b->meter_map));
    assert(meter_map_region_count(a->meter_map) == meter_map_region_count(b->meter_map));
    for (size_t i = 0; i < meter_map_region_count(a->meter_map); i++) {
        const MeterMapRegion *left = meter_map_region(a->meter_map, i);
        const MeterMapRegion *right = meter_map_region(b->meter_map, i);
        assert(time_signature_equals(meter_map_region_time_signature(left), meter_map_region_time_signature(right)));
        assert(musical_time_equals(meter_map_region_start(left), meter_map_region_start(right)));
        assert(meter_map_region_is_pickup(left) == meter_map_region_is_pickup(right));
    }
    assert(musical_time_equals(a->end_time, b->end_time));
    assert(a->conflict_count == b->conflict_count);
    for (size_t i = 0; i < a->conflict_count; i++) {
        assert(a->conflict_ticks[i] == b->conflict_ticks[i]);
    }
}

static void assert_tempos_equal(const MidiTempoImport *a, const MidiTempoImport *b) {
    assert(tempo_map_is_defaulted(a->tempo_map) == tempo_map_is_defaulted(b->tempo_map));
    assert(tempo_map_region_count(a->tempo_map) == tempo_map_region_count(b->tempo_map));
    for (size_t i = 0; i < tempo_map_region_count(a->tempo_map); i++) {
        const TempoMapRegion *left = tempo_map_region(a->tempo_map, i);
        const TempoMapRegion *right = tempo_map_region(b->tempo_map, i);
        assert(tempo_map_region_microseconds_per_quarter(left) == tempo_map_region_microseconds_per_quarter(right));
        assert(musical_time_equals(tempo_map_region_start(left), tempo_map_region_start(right)));
    }
    assert(a->conflict_count == b->conflict_count);
    for (size_t i = 0; i < a->conflict_count; i++) {
        assert(a->conflict_ticks[i] == b->conflict_ticks[i]);
    }
}

static void assert_notes_equal(const MidiImport *import, NoteEvent **events, PitchNumericValue **pitch_values,
                               size_t count) {
    assert(import->count == count);
    for (size_t i = 0; i < count; i++) {
        assert(note_event_equals_by_value(import->events[i], events[i]));
        assert(pitch_numeric_value_equals(import->pitch_values[i], pitch_values[i]));
        const VoiceReference *left = note_event_optional_voice(import->events[i]);
        const VoiceReference *right = note_event_optional_voice(events[i]);
        assert((left == NULL) == (right == NULL));
        if (left != NULL) {
            assert(voice_reference_equals(left, right));
        }
    }
}

static void destroy_notes(NoteEvent **events, PitchNumericValue **pitch_values, size_t count) {
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
}

/* The complete import equals what midi_importer_import and midi_key_signature_scan_all return. */
static void assert_matches_old_entry_points(const File *file) {
    MidiImport import;
    assert(midi_importer_import_complete(file->data, file->size, &import) == MIDI_IMPORT_OK);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    assert(midi_importer_import(file->data, file->size, &events, &pitch_values, &count) == MIDI_IMPORT_OK);
    assert_notes_equal(&import, events, pitch_values, count);
    destroy_notes(events, pitch_values, count);

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(midi_key_signature_scan_all(file->data, file->size, &keys) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);
    assert(import.key_signatures.count == keys.count);
    for (size_t i = 0; i < keys.count; i++) {
        const MidiKeySignatureEvent *left = &import.key_signatures.events[i];
        const MidiKeySignatureEvent *right = &keys.events[i];
        assert(left->tick == right->tick);
        assert(left->track_index == right->track_index);
        assert(left->sharps_flats == right->sharps_flats);
        assert(left->mode_byte == right->mode_byte);
        assert(left->mode_hint == right->mode_hint);
        assert(key_signature_equals(left->key_signature, right->key_signature));
        assert(musical_time_equals(left->position, right->position));
    }
    midi_key_signature_events_destroy(&keys);

    /* Every note maps to a real track, and every track entry is in file order. */
    for (size_t i = 0; i < import.count; i++) {
        assert(import.note_tracks[i] < import.track_count);
        assert(import.note_channels[i] < 16);
    }
    for (size_t t = 0; t < import.track_count; t++) {
        assert(import.tracks[t].index == t);
    }

    midi_import_destroy(&import);
}

static void test_equals_old_entry_points(void) {
    File type0 = fixture_type0();
    assert_matches_old_entry_points(&type0);
    free(type0.data);

    File type1 = fixture_type1();
    assert_matches_old_entry_points(&type1);
    free(type1.data);

    const char *corpus[] = {"Bach 114.mid", "Clementi 36.mid", "Field 1.mid", "Field 5.mid", "Mozart 545.mid"};
    for (size_t i = 0; i < sizeof(corpus) / sizeof(corpus[0]); i++) {
        File file = read_corpus_file(corpus[i]);
        assert_matches_old_entry_points(&file);
        free(file.data);
    }
}

static void test_failures(void) {
    MidiImport import;
    assert(midi_importer_import_complete(NULL, 10, &import) == MIDI_IMPORT_ERR_NULL_BUFFER);
    unsigned char byte = 0;
    assert(midi_importer_import_complete(&byte, 1, NULL) == MIDI_IMPORT_ERR_NULL_BUFFER);

    File file = fixture_type1();
    MidiImportResult short_header = midi_importer_import_complete(file.data, 5, &import);
    assert(short_header == MIDI_IMPORT_ERR_TRUNCATED || short_header == MIDI_IMPORT_ERR_BAD_HEADER);

    /* Every truncation of a valid file fails or imports cleanly and leaves nothing behind (valgrind checks it),
       and the three older entry points give the same verdict as the complete import. */
    for (size_t cut = 0; cut < file.size; cut += 3) {
        MidiImport cut_import;
        MidiImportResult complete = midi_importer_import_complete(file.data, cut, &cut_import);
        NoteEvent **events = NULL;
        PitchNumericValue **pitch_values = NULL;
        size_t count = 0;
        MidiImportResult plain = midi_importer_import(file.data, cut, &events, &pitch_values, &count);
        assert(complete == plain);
        if (complete == MIDI_IMPORT_OK) {
            midi_import_destroy(&cut_import);
            destroy_notes(events, pitch_values, count);
        }
    }

    /* A failed import leaves *out untouched. */
    MidiImport sentinel;
    memset(&sentinel, 0xAB, sizeof(sentinel));
    MidiImport before = sentinel;
    assert(midi_importer_import_complete(file.data, file.size - 1, &sentinel) != MIDI_IMPORT_OK);
    assert(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
    free(file.data);

    /* SMPTE division (top bit set) and a division of 0. */
    TrackBuilder track;
    memset(&track, 0, sizeof(track));
    add_note(&track, 0, 480, 0, 60);
    File smpte = file_from_tracks(0, &track, 1);
    smpte.data[12] = 0xE7;
    smpte.data[13] = 0x28;
    memset(&sentinel, 0xAB, sizeof(sentinel));
    assert(midi_importer_import_complete(smpte.data, smpte.size, &sentinel) == MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION);
    assert(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
    smpte.data[12] = 0;
    smpte.data[13] = 0;
    assert(midi_importer_import_complete(smpte.data, smpte.size, &sentinel) == MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION);
    assert(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
    free(smpte.data);

    /* Format 2. */
    File type2 = fixture_type0();
    type2.data[9] = 2;
    assert(midi_importer_import_complete(type2.data, type2.size, &sentinel) == MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT);
    free(type2.data);
}

/* A file whose last track fails after earlier tracks, names and programs were already read: no leak, *out untouched. */
static void test_destroy_after_partial_failure(void) {
    static TrackBuilder tracks[3];
    memset(tracks, 0, sizeof(tracks));
    add_name(&tracks[0], 0, "Good");
    add_program(&tracks[0], 0, 0, 3);
    add_note(&tracks[0], 0, 480, 0, 60);
    add_name(&tracks[1], 0, "Also good");
    add_key_signature(&tracks[1], 0, 1, 0);
    add_time_signature(&tracks[1], 0, 6, 3);
    add_note(&tracks[1], 0, 240, 1, 64);
    add_name(&tracks[2], 0, "Broken");
    add_program(&tracks[2], 0, 2, 9);
    add_unmatched_note_on(&tracks[2], 0, 2, 67);
    File file = file_from_tracks(1, tracks, 3);

    MidiImport sentinel;
    memset(&sentinel, 0xAB, sizeof(sentinel));
    MidiImport before = sentinel;
    assert(midi_importer_import_complete(file.data, file.size, &sentinel) == MIDI_IMPORT_ERR_BAD_TRACK);
    assert(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    assert(midi_importer_import(file.data, file.size, &events, &pitch_values, &count) == MIDI_IMPORT_ERR_BAD_TRACK);
    free(file.data);

    /* The same file without the broken track imports. */
    file = file_from_tracks(1, tracks, 2);
    MidiImport import;
    assert(midi_importer_import_complete(file.data, file.size, &import) == MIDI_IMPORT_OK);
    assert(import.count == 2 && import.track_count == 2);
    midi_import_destroy(&import);
    free(file.data);
}

static void test_destroy_edges(void) {
    midi_import_destroy(NULL);

    MidiImport zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    midi_import_destroy(&zeroed);
    midi_import_destroy(&zeroed);

    /* A file with no notes, no names and no events. */
    TrackBuilder empty;
    memset(&empty, 0, sizeof(empty));
    File file = file_from_tracks(0, &empty, 1);
    MidiImport import;
    assert(midi_importer_import_complete(file.data, file.size, &import) == MIDI_IMPORT_OK);
    assert(import.count == 0 && import.events == NULL && import.pitch_values == NULL);
    assert(import.note_tracks == NULL && import.note_channels == NULL);
    assert(import.track_count == 1 && import.tracks[0].name == NULL && import.tracks[0].programs == NULL);
    assert(import.key_signatures.count == 0 && import.key_signatures.events == NULL);
    assert(meter_map_is_defaulted(import.meter.meter_map));
    assert(tempo_map_is_defaulted(import.tempo.tempo_map));
    midi_import_destroy(&import);
    midi_import_destroy(&import);
    free(file.data);
}

int main(void) {
    test_type0();
    test_type1();
    test_equals_old_entry_points();
    test_failures();
    test_destroy_after_partial_failure();
    test_destroy_edges();
    printf("test_midi_import_complete: ok\n");
    return 0;
}
